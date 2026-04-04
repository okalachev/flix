// Fuzzy-PID gain scheduler
//
// Architecture: the fuzzy system does NOT replace the PID — it adjusts its
// gains (kP, kI, kD) in real time based on error magnitude and error rate.
// The inner PID loop stays intact, preserving the 1000 Hz timing budget.
//
// Inputs  → error (e), error rate (de/dt)
// Outputs → gain multipliers (scaleP, scaleI, scaleD) applied to base PID gains
//
// To activate: set USE_FUZZY_PID 1 in control.ino
// To disable:  set USE_FUZZY_PID 0  (reverts to plain PID, no overhead)
//
// Status: SKELETON — membership functions and rule base need tuning.
//         Behavior with default values is identical to plain PID (all scales = 1).

#pragma once

// ---------------------------------------------------------------------------
// Membership functions (triangular)
// Each returns a value in [0, 1] indicating degree of membership.
// ---------------------------------------------------------------------------

// Triangle membership: 0 outside [left, right], peak at center
static float trimf(float x, float left, float center, float right) {
    if (x <= left || x >= right) return 0.0f;
    if (x <= center) return (x - left) / (center - left);
    return (right - x) / (right - center);
}

// Shoulder-left membership: 1 below left, ramp down to 0 at right
static float lshoulder(float x, float left, float right) {
    if (x <= left) return 1.0f;
    if (x >= right) return 0.0f;
    return (right - x) / (right - left);
}

// Shoulder-right membership: 0 below left, ramp up to 1 at right
static float rshoulder(float x, float left, float right) {
    if (x <= left) return 0.0f;
    if (x >= right) return 1.0f;
    return (x - left) / (right - left);
}

// ---------------------------------------------------------------------------
// Linguistic labels for error (e) and error rate (de)
// Universe of discourse: adjust ranges to your drone's actual error magnitudes
// ---------------------------------------------------------------------------

// Error universe (radians for attitude, rad/s for rates)
#define E_NB_L  -1.0f   // Negative Big
#define E_NB_R  -0.3f
#define E_NS_L  -0.5f   // Negative Small
#define E_NS_C  -0.2f
#define E_NS_R   0.0f
#define E_ZE_L  -0.2f   // Zero
#define E_ZE_C   0.0f
#define E_ZE_R   0.2f
#define E_PS_L   0.0f   // Positive Small
#define E_PS_C   0.2f
#define E_PS_R   0.5f
#define E_PB_L   0.3f   // Positive Big
#define E_PB_R   1.0f

// Error rate universe (rad/s²)
#define DE_NB_L  -5.0f
#define DE_NB_R  -1.5f
#define DE_NS_L  -3.0f
#define DE_NS_C  -1.0f
#define DE_NS_R   0.0f
#define DE_ZE_L  -1.0f
#define DE_ZE_C   0.0f
#define DE_ZE_R   1.0f
#define DE_PS_L   0.0f
#define DE_PS_C   1.0f
#define DE_PS_R   3.0f
#define DE_PB_L   1.5f
#define DE_PB_R   5.0f

// ---------------------------------------------------------------------------
// FuzzyGainScheduler
// Wraps a PID and adjusts its P, I, D gains every call via fuzzy rules.
// ---------------------------------------------------------------------------
class FuzzyGainScheduler {
public:
    float p, i, d;          // base gains (same as PID constructor)
    float windup;
    float dtMax;

    float derivative = 0;
    float integral   = 0;

    FuzzyGainScheduler(float p, float i, float d,
                       float windup = 0, float dAlpha = 1, float dtMax = 0.1)
        : p(p), i(i), d(d), windup(windup), dtMax(dtMax) {}

    // Same interface as PID::update() — drop-in replacement
    float update(float error) {
        float dt = t - prevTime;

        if (dt > 0 && dt < dtMax) {
            float dError = (error - prevError) / dt;
            integral += error * dt;

            // --- Compute fuzzy gain multipliers ---
            float scaleP, scaleI, scaleD;
            computeGains(error, dError, scaleP, scaleI, scaleD);

            // --- Apply adjusted gains ---
            float pTerm = (p * scaleP) * error;
            float iTerm = constrain((i * scaleI) * integral, -windup, windup);
            float dTerm = (d * scaleD) * dError;

            derivative = dError;

            prevError = error;
            prevTime  = t;
            return pTerm + iTerm + dTerm;
        }

        // Reset on timing gap (same behavior as PID)
        integral   = 0;
        derivative = 0;
        prevError  = error;
        prevTime   = t;
        return p * error;
    }

    void reset() {
        prevError  = NAN;
        prevTime   = NAN;
        integral   = 0;
        derivative = 0;
    }

private:
    float prevError = NAN;
    float prevTime  = NAN;

    // -----------------------------------------------------------------------
    // Fuzzy rule base — Sugeno-style (weighted average of crisp outputs)
    //
    // Rule table for scaleP (example — tune all three tables):
    //
    //         |  de: NB  NS  ZE  PS  PB
    //  -----  |  ---------------------
    //  e: NB  |   0.5  0.6  0.8  1.0  1.2
    //  e: NS  |   0.6  0.8  1.0  1.1  1.2
    //  e: ZE  |   0.8  0.9  1.0  0.9  0.8
    //  e: PS  |   1.2  1.1  1.0  0.8  0.6
    //  e: PB  |   1.2  1.0  0.8  0.6  0.5
    //
    // Values > 1 → increase gain, < 1 → decrease gain, 1 → unchanged
    // TODO: tune these tables using simulator data
    // -----------------------------------------------------------------------
    void computeGains(float e, float de, float& scaleP, float& scaleI, float& scaleD) {
        // Fuzzify error
        float eNB = lshoulder(e,  E_NB_L, E_NB_R);
        float eNS = trimf(e,      E_NS_L, E_NS_C, E_NS_R);
        float eZE = trimf(e,      E_ZE_L, E_ZE_C, E_ZE_R);
        float ePS = trimf(e,      E_PS_L, E_PS_C, E_PS_R);
        float ePB = rshoulder(e,  E_PB_L, E_PB_R);

        // Fuzzify error rate
        float deNB = lshoulder(de, DE_NB_L, DE_NB_R);
        float deNS = trimf(de,     DE_NS_L, DE_NS_C, DE_NS_R);
        float deZE = trimf(de,     DE_ZE_L, DE_ZE_C, DE_ZE_R);
        float dePS = trimf(de,     DE_PS_L, DE_PS_C, DE_PS_R);
        float dePB = rshoulder(de, DE_PB_L, DE_PB_R);

        // --- scaleP rule table (5x5) ---
        // TODO: tune output values
        const float ruleP[5][5] = {
            {0.5f, 0.6f, 0.8f, 1.0f, 1.2f},  // e=NB
            {0.6f, 0.8f, 1.0f, 1.1f, 1.2f},  // e=NS
            {0.8f, 0.9f, 1.0f, 0.9f, 0.8f},  // e=ZE
            {1.2f, 1.1f, 1.0f, 0.8f, 0.6f},  // e=PS
            {1.2f, 1.0f, 0.8f, 0.6f, 0.5f},  // e=PB
        };

        // --- scaleI rule table (5x5) ---
        // TODO: tune output values
        const float ruleI[5][5] = {
            {0.2f, 0.3f, 0.5f, 0.7f, 0.9f},
            {0.3f, 0.5f, 0.7f, 0.8f, 0.9f},
            {0.5f, 0.7f, 1.0f, 0.7f, 0.5f},
            {0.9f, 0.8f, 0.7f, 0.5f, 0.3f},
            {0.9f, 0.7f, 0.5f, 0.3f, 0.2f},
        };

        // --- scaleD rule table (5x5) ---
        // TODO: tune output values
        const float ruleD[5][5] = {
            {1.2f, 1.1f, 0.8f, 0.6f, 0.4f},
            {1.1f, 1.0f, 0.7f, 0.6f, 0.5f},
            {0.8f, 0.7f, 1.0f, 0.7f, 0.8f},
            {0.5f, 0.6f, 0.7f, 1.0f, 1.1f},
            {0.4f, 0.6f, 0.8f, 1.1f, 1.2f},
        };

        float eLabels[5]  = {eNB,  eNS,  eZE,  ePS,  ePB};
        float deLabels[5] = {deNB, deNS, deZE, dePS, dePB};

        // Defuzzify: weighted average (Sugeno)
        scaleP = weightedAverage(eLabels, deLabels, ruleP);
        scaleI = weightedAverage(eLabels, deLabels, ruleI);
        scaleD = weightedAverage(eLabels, deLabels, ruleD);
    }

    float weightedAverage(float eL[5], float deL[5], const float rules[5][5]) {
        float num = 0, den = 0;
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 5; j++) {
                float w = eL[i] * deL[j]; // AND = product
                num += w * rules[i][j];
                den += w;
            }
        }
        return (den > 0) ? (num / den) : 1.0f; // default scale = 1 (no change)
    }
};
