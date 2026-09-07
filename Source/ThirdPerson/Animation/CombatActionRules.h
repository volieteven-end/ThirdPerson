#pragma once

// Pure action rules shared by runtime code and automation tests.
namespace TPCActionRules
{
inline bool LocksMovement(bool Dead, bool Hit, bool Guard, bool Attack, bool Dodge)
{
    return Dead || Hit || Guard || Attack || Dodge;
}

// Attacks lock WASD, but may be interrupted by a dodge. Hits/guard/death/dodges may not.
inline bool BlocksDodge(bool Dead, bool Hit, bool Guard, bool Dodge)
{
    return Dead || Hit || Guard || Dodge;
}

// The window starts at the natural end of the FIRST dodge, not at input time.
struct FDashComboWindow
{
    int NextIndex = -1;
    bool bDashing = false;
    bool bQueued = false;
    double ExpiresAt = 0.0;
    void Reset() { NextIndex = -1; bDashing = bQueued = false; ExpiresAt = 0.0; }
    void Arm(int Index) { Reset(); NextIndex = Index; bDashing = true; }
    bool IsOpen(double Now) const { return NextIndex >= 0 && (bDashing || Now <= ExpiresAt); }
    bool Queue(double Now)
    {
        if (!bDashing || !IsOpen(Now)) { return false; }
        bQueued = true;
        return true;
    }
    void Finish(double Now, double Duration)
    {
        if (NextIndex < 0 || !bDashing) { return; }
        if (Duration <= 0.0) { Reset(); return; }
        bDashing = false;
        ExpiresAt = Now + Duration;
    }
    int Consume(double Now)
    {
        if (bDashing) { return -1; }
        const int Index = IsOpen(Now) ? NextIndex : -1;
        Reset();
        return Index;
    }
};

// A single intent belongs to exactly one action generation. Early clicks are
// retained until its chain point; interruption never carries them to a new action.
struct FComboBuffer
{
    unsigned long long Generation = 0;
    bool bQueued = false;
    bool bChainPointReached = false;
    bool bConsumed = false;

    void Reset(unsigned long long InGeneration)
    {
        Generation = InGeneration;
        bQueued = bChainPointReached = bConsumed = false;
    }
    void Queue() { if (!bConsumed) { bQueued = true; } }
    bool Consume(unsigned long long ExpectedGeneration, bool bCanChain)
    {
        if (ExpectedGeneration != Generation || !bCanChain || !bQueued ||
            !bChainPointReached || bConsumed)
        {
            return false;
        }
        bQueued = false;
        bConsumed = true;
        return true;
    }
};
}
