#include "ActionDefinition.h"

const UActionDefinition* UActionSet::FindByMontage(const UAnimMontage* Montage) const
{
    if (!Montage) return nullptr;
    for (const auto* Moves : { &GroundCombo, &AirCombo, &Dodges, &Turns })
        for (const UActionDefinition* Move : *Moves)
            if (Move && Move->Montage == Montage) return Move;
    for (const UActionDefinition* Move : { Rising.Get(), Dive.Get(), SprintAttack.Get(), ParryCounter.Get(),
                                          Buff.Get(), DrawWeapon.Get(), SheatheWeapon.Get() })
        if (Move && Move->Montage == Montage) return Move;
    return nullptr;
}
