"""Temporary local, file-based work queue executed inside Unreal Editor.

Only scripts inside this build directory can be submitted. No socket, plugin,
project setting or startup hook is installed. The stop command unregisters it.
"""
import contextlib
import io
import json
import pathlib
import time
import traceback
import unreal

_rc_root = pathlib.Path(__file__).resolve().parent
_rc_request = _rc_root / 'request.json'
_rc_response = _rc_root / 'response.json'
_rc_seen = set()
_rc_next = 0.0
_rc_handle = None

def _rc_tick(delta):
    global _rc_next, _rc_handle
    if time.monotonic() < _rc_next:
        return
    _rc_next = time.monotonic() + 0.2
    if not _rc_request.exists():
        return
    try:
        job = json.loads(_rc_request.read_text(encoding='utf-8-sig'))
        jid = job['id']
        if jid in _rc_seen:
            return
        _rc_seen.add(jid)
        start = time.time()
        capture = io.StringIO()
        result = {'id': jid, 'success': False}
        try:
            if job.get('stop'):
                unreal.unregister_slate_post_tick_callback(_rc_handle)
                result.update(success=True, output='BRIDGE_STOPPED')
            else:
                script = (_rc_root / job['script']).resolve()
                if _rc_root not in script.parents or script.suffix != '.py':
                    raise ValueError('Script must be a Python file in this build directory')
                with contextlib.redirect_stdout(capture), contextlib.redirect_stderr(capture):
                    exec(compile(script.read_text(encoding='utf-8-sig'), str(script), 'exec'), {'__name__':'__main__', '__file__':str(script), 'JOB':job})
                result.update(success=True, output=capture.getvalue())
        except Exception:
            result.update(output=capture.getvalue(), error=traceback.format_exc())
        result['seconds'] = round(time.time() - start, 3)
        _rc_response.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
        unreal.log('ROYAL_CAPITAL_JOB ' + str(jid) + ' ' + str(result['success']))
    except Exception:
        unreal.log_error(traceback.format_exc())

_rc_handle = unreal.register_slate_post_tick_callback(_rc_tick)
(_rc_root / 'bridge_ready.json').write_text(json.dumps({'ready':True,'engine':unreal.SystemLibrary.get_engine_version()}), encoding='utf-8')
unreal.log('ROYAL_CAPITAL_BRIDGE_READY')
