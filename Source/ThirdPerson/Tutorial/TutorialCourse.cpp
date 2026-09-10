#include "TutorialCourse.h"

void UTutorialCourse::PopulateDefaults(UTutorialCourse& C)
{
    C.Lessons.Reset();
    C.ReturnMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson.Lvl_ThirdPerson")));
    auto Lesson = [&C](const TCHAR* Id, const TCHAR* Title, const TCHAR* Intro, bool Optional = false) -> FTutorialLesson&
    {
        auto& L = C.Lessons.AddDefaulted_GetRef(); L.Id = Id; L.Title = FText::FromString(Title);
        L.Introduction = FText::FromString(Intro); L.bOptional = Optional; return L;
    };
    auto Goal = [](FTutorialLesson& L, ETutorialSignal S, const TCHAR* Text, const TCHAR* Keys, const TCHAR* Target = TEXT(""), int32 Number = 1)
    {
        auto& O = L.Objectives.AddDefaulted_GetRef(); O.Signal = S; O.Description = FText::FromString(Text);
        O.Keys = FText::FromString(Keys); O.TargetId = FName(Target); O.RequiredCount = Number;
    };
    auto& Move = Lesson(TEXT("Movement"), TEXT("01  入营 · 移动"), TEXT("沿金色石路前进，完成快跑和跳跃。短按 Shift 冲刺，长按 Shift 快跑，空格跳跃。"));
    Goal(Move, ETutorialSignal::WalkMarker, TEXT("走到第一个金色路标"), TEXT("W A S D  移动  ·  鼠标  看向"), TEXT("Walk01"));
    Goal(Move, ETutorialSignal::WalkMarker, TEXT("沿石路走到第二个路标"), TEXT("W A S D  移动"), TEXT("Walk02"));
    Goal(Move, ETutorialSignal::SprintMarker, TEXT("持续快跑穿过前方金色路标"), TEXT("移动 + 长按 Shift  快跑"), TEXT("Sprint"));
    Goal(Move, ETutorialSignal::JumpLanding, TEXT("按空格跳过矮墙，落在金色平台上"), TEXT("空格  跳跃"), TEXT("Jump"));
    auto& Sword = Lesson(TEXT("Sword"), TEXT("02  剑术 · 四段连击"), TEXT("靠近训练目标，攻击必须真正命中。中键可锁定目标。"));
    Goal(Sword, ETutorialSignal::SwordHit, TEXT("用普通攻击命中训练目标一次"), TEXT("左键  攻击  ·  中键  锁定"), TEXT("SwordTarget"));
    Goal(Sword, ETutorialSignal::GroundCombo, TEXT("重新从第一刀开始，连续命中完整四段连击"), TEXT("连续左键  四段连击"), TEXT("SwordTarget"));
    auto& Dodge = Lesson(TEXT("Dodge"), TEXT("03  身法 · 闪避"), TEXT("短按 Shift 后立即松开即可冲刺闪避；长按 Shift 则快跑。先向前，再向侧面闪避。"));
    Goal(Dodge, ETutorialSignal::ForwardDodge, TEXT("朝前方金圈闪避，并进入圈内"), TEXT("W + 短按 Shift  冲刺闪避"), TEXT("DodgeForward"));
    Goal(Dodge, ETutorialSignal::SideDodge, TEXT("保持面朝前方，向右侧金圈闪避"), TEXT("D + 短按 Shift  冲刺闪避"), TEXT("DodgeSide"));
    auto& Guard = Lesson(TEXT("Guard"), TEXT("04  守势 · 格挡与弹反"), TEXT("面向陪练，等它出招。长按右键格挡；命中前重新按右键可弹反。"));
    Goal(Guard, ETutorialSignal::Blocked, TEXT("面向陪练，成功格挡两次攻击"), TEXT("提前按住右键  格挡"), TEXT("GuardTarget"), 2);
    Goal(Guard, ETutorialSignal::Parried, TEXT("松开右键，在攻击将要命中时按下，完成一次弹反"), TEXT("命中前按右键  弹反"), TEXT("GuardTarget"));
    auto& Potion = Lesson(TEXT("Potion"), TEXT("05  补给 · 血瓶"), TEXT("训练已将生命设为一半。使用一瓶药，观察生命和药瓶数量变化。"));
    Goal(Potion, ETutorialSignal::PotionUsed, TEXT("使用一瓶药并恢复生命"), TEXT("Q  使用血瓶"));
    auto& Combat = Lesson(TEXT("Combat"), TEXT("06  演练 · 实战"), TEXT("依次击败两名低伤害陪练。不会失去正式存档、获得经验或掉落物品。"));
    Goal(Combat, ETutorialSignal::EnemyDefeated, TEXT("击败第一名陪练，再迎战第二名"), TEXT("左键攻击  ·  短按 Shift 闪避  ·  右键防御  ·  Q 血瓶"), TEXT("CombatTarget"), 2);
    auto& Air = Lesson(TEXT("Air"), TEXT("进阶  升龙 · 空连 · 下砸"), TEXT("这是可选练习，不影响基础教学完成。先分步练习，再连成一套。"), true);
    Goal(Air, ETutorialSignal::RisingHit, TEXT("靠近目标，用升龙命中并腾空"), TEXT("按住 C + 左键  升龙"), TEXT("AirTarget"));
    Goal(Air, ETutorialSignal::AirPair, TEXT("腾空后依次打出两段空中攻击"), TEXT("空格跳跃 / 升龙后，连续左键"));
    Goal(Air, ETutorialSignal::DiveHit, TEXT("靠近目标，在空中使用下砸，以落地冲击命中"), TEXT("空中 R  下砸  ·  训练冲击半径 1.2 米"), TEXT("AirTarget"));
    Goal(Air, ETutorialSignal::AirChain, TEXT("一套完成：升龙命中 → 空中两刀 → 下砸命中"), TEXT("C + 左键 → 左键两段 → R"), TEXT("AirTarget"));
}

bool UTutorialCourse::IsValidCourse(FString& Error) const
{
    if (Lessons.IsEmpty()) { Error = TEXT("Course has no lessons"); return false; }
    TSet<FName> Ids; bool OptionalSeen = false;
    for (const auto& L : Lessons)
    {
        if (L.Id.IsNone() || Ids.Contains(L.Id) || L.Objectives.IsEmpty()) { Error = TEXT("Invalid lesson ID or empty objectives"); return false; }
        Ids.Add(L.Id); OptionalSeen |= L.bOptional;
        if (OptionalSeen && !L.bOptional) { Error = TEXT("Optional lessons must follow the basics"); return false; }
        for (const auto& O : L.Objectives) if (O.RequiredCount < 1 || O.Description.IsEmpty()) { Error = TEXT("Invalid objective"); return false; }
    }
    Error.Reset(); return true;
}

void FTutorialProgress::Initialize(const UTutorialCourse* InCourse)
{
    Course = InCourse; Status.Init(ETutorialLessonStatus::Locked, Course ? Course->Lessons.Num() : 0);
    Lesson = INDEX_NONE; Objective = Count = 0; Seen.Reset(); bPractice = false;
    if (!Status.IsEmpty()) { Status[0] = ETutorialLessonStatus::Available; StartLesson(0); }
}
bool FTutorialProgress::IsUnlocked(int32 Index) const { return Status.IsValidIndex(Index) && Status[Index] != ETutorialLessonStatus::Locked; }
bool FTutorialProgress::StartLesson(int32 Index, bool Practice)
{
    if (!Course || !IsUnlocked(Index)) return false;
    Lesson = Index; bPractice = Practice; Retry(); return true;
}
void FTutorialProgress::Retry() { Objective = Count = 0; Seen.Reset(); }
bool FTutorialProgress::BasicsFinished() const
{
    if (!Course || Status.Num() != Course->Lessons.Num()) return false;
    for (int32 I = 0; I < Status.Num(); ++I)
        if (!Course->Lessons[I].bOptional && Status[I] != ETutorialLessonStatus::Completed && Status[I] != ETutorialLessonStatus::Skipped) return false;
    return !Status.IsEmpty();
}
const FTutorialObjective* FTutorialProgress::CurrentObjective() const
{
    return Course && Course->Lessons.IsValidIndex(Lesson) && Course->Lessons[Lesson].Objectives.IsValidIndex(Objective)
        ? &Course->Lessons[Lesson].Objectives[Objective] : nullptr;
}
ETutorialProgressResult FTutorialProgress::Observe(ETutorialSignal Signal, uint64 Serial, FName TargetId)
{
    const auto* O = CurrentObjective();
    if (!O || !Serial || O->Signal != Signal || (!O->TargetId.IsNone() && O->TargetId != TargetId)) return ETutorialProgressResult::Ignored;
    const FString Key = FString::Printf(TEXT("%d:%llu:%s"), int32(Signal), Serial, *TargetId.ToString());
    if (Seen.Contains(Key)) return ETutorialProgressResult::Ignored;
    Seen.Add(Key);
    if (++Count < O->RequiredCount) return ETutorialProgressResult::Counted;
    Count = 0; ++Objective;
    if (CurrentObjective()) return ETutorialProgressResult::ObjectiveCompleted;
    Finish(false); return ETutorialProgressResult::LessonCompleted;
}
void FTutorialProgress::Skip() { if (CurrentObjective()) Finish(true); }
void FTutorialProgress::Finish(bool Skipped)
{
    if (!Course || !Status.IsValidIndex(Lesson)) return;
    const int32 Previous = Lesson;
    if (Status[Previous] != ETutorialLessonStatus::Completed)
        Status[Previous] = Skipped ? ETutorialLessonStatus::Skipped : ETutorialLessonStatus::Completed;
    Lesson = INDEX_NONE; Objective = Count = 0; Seen.Reset();
    if (bPractice || Course->Lessons[Previous].bOptional) return;
    const int32 Next = Previous + 1;
    if (Status.IsValidIndex(Next))
    {
        Status[Next] = ETutorialLessonStatus::Available;
        if (!Course->Lessons[Next].bOptional) StartLesson(Next);
    }
    if (BasicsFinished())
        for (int32 I = 0; I < Status.Num(); ++I)
            if (Course->Lessons[I].bOptional && Status[I] == ETutorialLessonStatus::Locked) Status[I] = ETutorialLessonStatus::Available;
}
