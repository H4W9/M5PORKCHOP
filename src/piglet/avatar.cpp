// Piglet ASCII avatar implementation

#include "avatar.h"
#include "weather.h"
#include "../ui/display.h"
#include <time.h>

// Static members
AvatarState Avatar::currentState = AvatarState::NEUTRAL;
bool Avatar::isBlinking = false;
bool Avatar::earsUp = true;
uint32_t Avatar::lastBlinkTime = 0;
uint32_t Avatar::blinkInterval = 3000;
int Avatar::moodIntensity = 0;  // Phase 8: -100 to 100

// Cute jump state
bool Avatar::jumpActive = false;
uint32_t Avatar::jumpStartTime = 0;
// Pig-animation state (stage 3)
bool     Avatar::attackHopActive = false;
uint32_t Avatar::attackHopStartTime = 0;
uint8_t  Avatar::attackHopIndex = 0;
uint8_t  Avatar::attackHopTotal = 0;
int16_t  Avatar::attackHopOriginX = 0;
int16_t  Avatar::attackHopTargets[5] = {0};
bool     Avatar::spinActive = false;
uint32_t Avatar::spinStart = 0;
bool     Avatar::perkUpActive = false;
uint32_t Avatar::perkUpStart = 0;
bool     Avatar::flinchActive = false;
uint32_t Avatar::flinchStart = 0;
bool     Avatar::pawScratchActive = false;
uint32_t Avatar::pawScratchStart = 0;
bool     Avatar::tailWiggleActive = false;
uint32_t Avatar::tailWiggleStart = 0;
Avatar::SparkleParticle Avatar::sparkles[Avatar::MAX_SPARKLES] = {};

// Walk transition state
bool Avatar::transitioning = false;
uint32_t Avatar::transitionStartTime = 0;
int Avatar::transitionFromX = 2;
int Avatar::transitionToX = 2;
bool Avatar::transitionToFacingRight = true;
int Avatar::currentX = 2;

// Sniff animation state
bool Avatar::isSniffing = false;
static uint32_t sniffStartTime = 0;
static const uint32_t SNIFF_DURATION_MS = 600;  // 600ms for proper sniff cycle
static uint8_t sniffFrame = 0;  // Alternates between nose shapes (oo, oO, Oo)

// Walk transition timing
static const uint32_t TRANSITION_DURATION_MS = 1200;  // 1.2s slow relaxed walk (was 400ms - too hectic)

// Rest cooldown after grass stops - prevents immediate re-triggering
static uint32_t lastGrassStopTime = 0;
static const uint32_t GRASS_REST_COOLDOWN_MS = 3000;  // 3 second chill period after grass stops

// Attack shake state (visual feedback for captures)
static bool attackShakeActive = false;
static bool attackShakeStrong = false;
static uint32_t attackShakeRefreshTime = 0;

// Thunder flash state (weather effect - invert colors)
static bool thunderFlashActive = false;

// Night sky star system state
Avatar::Star Avatar::stars[15] = {{0}};
uint8_t Avatar::starCount = 0;
uint32_t Avatar::lastStarSpawn = 0;
uint32_t Avatar::nextSpawnDelay = 2000;
bool Avatar::starsActive = false;
uint32_t Avatar::lastNightCheck = 0;
bool Avatar::cachedNightMode = false;

// Color helper for thunder flash inversion (matches Sirloin)
static uint16_t getDrawColor() {
    if (thunderFlashActive) {
        return getColorBG();  // Swap: draw with BG color during flash
    }
    return getColorFG();
}

static uint16_t getBGColor() {
    if (thunderFlashActive) {
        return getColorFG();  // Inverted while flashing
    }
    return getColorBG();
}

// True when the Realistic theme should tint scene elements. Suppressed during a
// thunder flash so the whole scene still inverts to the mono flash colour.
static inline bool realActive() {
    return !thunderFlashActive && isRealisticTheme();
}

// Grass animation state
bool Avatar::grassMoving = false;
bool Avatar::grassDirection = true;  // true = grass scrolls right
bool Avatar::pendingGrassStart = false;  // Wait for transition before starting grass
uint32_t Avatar::lastGrassUpdate = 0;
uint16_t Avatar::grassSpeed = 80;  // Default fast for OINK
Avatar::GrassBlade Avatar::grassBlades[Avatar::GRASS_BLADE_MAX] = {};
int16_t Avatar::grassOffset = 0;

// Trail particle system (dust kicked up by the running pig)
struct TrailParticle {
    float x, y, vx, vy, startX, maxDist;
    uint8_t baseSize;
    bool active;
};
static const int TRAIL_COUNT = 10;
static TrailParticle trailParticles[TRAIL_COUNT] = {};
static uint32_t lastTrailSpawn = 0;
static uint32_t lastTrailUpdate = 0;
static int trailSpawnIdx = 0;
// Internal state for looking direction
static bool facingRight = true;  // Default: pig looks right
static uint32_t lastFlipTime = 0;
static uint32_t flipInterval = 5000;

// Look behavior (stationary observation)
static uint32_t lastLookTime = 0;
static uint32_t lookInterval = 2000;  // Look around every 2-5s when stationary
bool Avatar::onRightSide = false;  // Track which side of screen pig is on (class static)

// --- DERPY STYLE with direction ---
// Right-looking frames (snout 00 on right side of face, pig looks RIGHT)
const char* AVATAR_NEUTRAL_R[] = {
    " ?  ? ",
    "(o 00)",
    "(    )"
};

const char* AVATAR_HAPPY_R[] = {
    " ^  ^ ",
    "(^ 00)",
    "(    )"
};

const char* AVATAR_EXCITED_R[] = {
    " !  ! ",
    "(@ 00)",
    "(    )"
};

const char* AVATAR_HUNTING_R[] = {
    " |  | ",
    "(= 00)",
    "(    )"
};

const char* AVATAR_SLEEPY_R[] = {
    " v  v ",
    "(- 00)",
    "(    )"
};

const char* AVATAR_SAD_R[] = {
    " .  . ",
    "(T 00)",
    "(    )"
};

const char* AVATAR_ANGRY_R[] = {
    " \\  / ",
    "(# 00)",
    "(    )"
};

// Left-looking frames (snout 00 on left side of face, pig looks LEFT, z pigtail)
const char* AVATAR_NEUTRAL_L[] = {
    " ?  ? ",
    "(00 o)",
    "(    )z"
};

const char* AVATAR_HAPPY_L[] = {
    " ^  ^ ",
    "(00 ^)",
    "(    )z"
};

const char* AVATAR_EXCITED_L[] = {
    " !  ! ",
    "(00 @)",
    "(    )z"
};

const char* AVATAR_HUNTING_L[] = {
    " |  | ",
    "(00 =)",
    "(    )z"
};

const char* AVATAR_SLEEPY_L[] = {
    " v  v ",
    "(00 -)",
    "(    )z"
};

const char* AVATAR_SAD_L[] = {
    " .  . ",
    "(00 T)",
    "(    )z"
};

const char* AVATAR_ANGRY_L[] = {
    " \\  / ",
    "(00 #)",
    "(    )z"
};

void Avatar::init() {
    currentState = AvatarState::NEUTRAL;
    isBlinking = false;
    isSniffing = false;
    earsUp = true;
    lastBlinkTime = millis();
    blinkInterval = random(4000, 8000);
    
    // Init direction - start at LEFT or RIGHT edge (not center)
    // This ensures bubble can float beside pig from the start
    bool startRight = random(0, 2) == 0;
    onRightSide = startRight;
    currentX = startRight ? 108 : 20;  // Start at proper edge position
    facingRight = !startRight;  // Face toward center (more interesting)
    lastFlipTime = millis();
    flipInterval = random(25000, 50000);  // First walk: 25-50s
    lastLookTime = millis();
    lookInterval = random(3000, 8000);  // First look: 3-8s (let pig settle in)
    
    // Init grass pattern - full screen width at size 2 (~24 chars)
    grassMoving = false;
    grassDirection = true;
    pendingGrassStart = false;
    grassSpeed = 80;
    lastGrassUpdate = millis();
    lastGrassStopTime = 0;  // No cooldown on fresh init
    grassOffset = 0;
    for (int i = 0; i < GRASS_BLADE_MAX; i++) {
        grassBlades[i].height = random(6, 20);
        grassBlades[i].lean   = random(-3, 4);
        grassBlades[i].width  = random(1, 4);
    }

    // Init star system, dormant until night
    starsActive = false;
    starCount = 0;
    lastStarSpawn = 0;
    nextSpawnDelay = 2000;
    lastNightCheck = 0;
    cachedNightMode = false;
    initStarPositions();

    // Fruit tree: start hidden (dropping-fruit/splash arrays are zero-init).
    treePhase = TreePhase::HIDDEN;
    treeScrollOffset = 0;
    treePendingHide = false;
    treePendingShow = false;

    // Remove duplicate grass pattern initialization - already done earlier in init()
}

void Avatar::setState(AvatarState state) {
    currentState = state;
}

void Avatar::setMoodIntensity(int intensity) {
    moodIntensity = constrain(intensity, -100, 100);
}

bool Avatar::isFacingRight() {
    return facingRight;
}

bool Avatar::isOnRightSide() {
    return onRightSide;
}

bool Avatar::isTransitioning() {
    return transitioning;
}

int Avatar::getCurrentX() {
    return currentX;
}

void Avatar::blink() {
    isBlinking = true;
}

void Avatar::wiggleEars() {
    earsUp = !earsUp;
}

void Avatar::sniff() {
    if (!isSniffing) {
        sniffFrame = 0;  // Reset frame on new sniff
    }
    isSniffing = true;
    sniffStartTime = millis();
}

void Avatar::cuteJump() {
    // Trigger a cute celebratory jump - higher and slower than walk bounce
    jumpActive = true;
    jumpStartTime = millis();
}

// ---- Upstream pig-animation triggers (stage 3) --------------------------------
void Avatar::perkUp() {
    if (attackHopActive || spinActive) return;
    perkUpActive = true; perkUpStart = millis();
}
void Avatar::flinch() {
    if (attackHopActive || spinActive) return;
    flinchActive = true; flinchStart = millis();
}
void Avatar::spin() {
    if (attackHopActive) return;
    jumpActive = false;  // spin owns the Y arc
    spinActive = true; spinStart = millis();
}
void Avatar::pawScratch() {
    if (attackHopActive || spinActive || perkUpActive || transitioning) return;
    pawScratchActive = true; pawScratchStart = millis();
}
void Avatar::triggerTailWiggle() {
    tailWiggleActive = true; tailWiggleStart = millis();
}
void Avatar::triggerSparkles(uint8_t count) {
    int cx = currentX + 40;  // rough pig centre
    int cy = 50;
    for (uint8_t i = 0; i < MAX_SPARKLES && count > 0; i++) {
        if (sparkles[i].life == 0) {
            sparkles[i].x = cx + random(-10, 11);
            sparkles[i].y = cy + random(-10, 11);
            sparkles[i].vx = random(-3, 4);
            sparkles[i].vy = random(-4, 1);  // bias upward
            sparkles[i].life = random(10, 18);
            count--;
        }
    }
}
bool Avatar::isAttackHopping() { return attackHopActive; }

void Avatar::attackHop() {
    attackHopActive = true;
    attackHopStartTime = millis();
    attackHopIndex = 0;
    attackHopOriginX = currentX;
    attackHopTotal = random(3, 6);  // 3-5 hops
    const int16_t hi = (int16_t)(DISPLAY_W / 2);  // clamp to left half (screen-relative)
    int16_t prevX = currentX;
    for (uint8_t i = 0; i < attackHopTotal; i++) {
        if (i == attackHopTotal - 1) {
            attackHopTargets[i] = attackHopOriginX;   // last hop returns home
        } else {
            int16_t offset = random(25, 56);
            if (random(0, 2) == 0) offset = -offset;
            int16_t target = prevX + offset;
            if (target < 10) target = 10;
            if (target > hi) target = hi;
            attackHopTargets[i] = target;
        }
        prevX = attackHopTargets[i];
    }
}

void Avatar::updateAndDrawSparkles(M5Canvas& canvas) {
    uint16_t fg = getColorFG();
    for (uint8_t i = 0; i < MAX_SPARKLES; i++) {
        if (sparkles[i].life == 0) continue;
        sparkles[i].x += sparkles[i].vx;
        sparkles[i].y += sparkles[i].vy;
        sparkles[i].life--;
        if (sparkles[i].life > 6) canvas.fillRect(sparkles[i].x, sparkles[i].y, 2, 2, fg);
        else                      canvas.drawPixel(sparkles[i].x, sparkles[i].y, fg);
    }
}

void Avatar::draw(M5Canvas& canvas) {
    uint32_t now = millis();
    
    // Sniff animation times out after SNIFF_DURATION_MS
    // Update sniff frame for animation (cycle every 100ms)
    if (isSniffing) {
        if (now - sniffStartTime > SNIFF_DURATION_MS) {
            isSniffing = false;
            sniffFrame = 0;
        } else {
            sniffFrame = ((now - sniffStartTime) / 100) % 3;  // 3 frames: oo, oO, Oo
        }
    }
    
    // Handle walk transition animation
    if (transitioning) {
        uint32_t elapsed = now - transitionStartTime;
        if (elapsed >= TRANSITION_DURATION_MS) {
            // Transition complete
            transitioning = false;
            currentX = transitionToX;
            facingRight = transitionToFacingRight;
            onRightSide = (currentX > 60);  // Track which side we're on
            
            // Start grass now if it was pending
            if (pendingGrassStart) {
                grassMoving = true;
                pendingGrassStart = false;
                facingRight = !grassDirection;  // Face opposite to grass movement
            } else {
                // === POST-WALK RANDOM BEHAVIOR ===
                // After arriving somewhere, pig might do something interesting
                int arrivalRoll = random(0, 100);
                if (arrivalRoll < 20) {
                    // 20%: Look around after arriving (curious)
                    facingRight = !facingRight;
                } else if (arrivalRoll < 35) {
                    // 15%: Sniff the new area
                    sniff();
                } else if (arrivalRoll < 45) {
                    // 10%: Quick ear wiggle (settling in)
                    wiggleEars();
                } else if (arrivalRoll < 55) {
                    // 10%: Turn around completely (changed mind)
                    facingRight = !transitionToFacingRight;
                }
                // 45%: Just face the direction we were walking
            }
            
            // Reset look timer for new position with random delay
            lastLookTime = now;
            lookInterval = random(1500, 6000);  // More variable
        } else {
            // Animate X position (ease in-out)
            float t = (float)elapsed / TRANSITION_DURATION_MS;
            // Heavy quintic ease: 6t^5 - 15t^4 + 10t^3 (more inertia than smooth step)
            // Flatter acceleration/deceleration = heavier feel
            float smoothT = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
            currentX = transitionFromX + (int)((transitionToX - transitionFromX) * smoothT);
        }
    }

    // Phase 8: Mood intensity affects animation timing
    // High positive = excited (faster blinks, more looking around)
    // High negative = lethargic (slower blinks, less movement)
    
    // Calculate intensity-adjusted blink interval
    // Base: 4000-8000ms, excited (-50%): 2000-4000ms, sad (+50%): 6000-12000ms
    float blinkMod = 1.0f - (moodIntensity / 200.0f);  // 0.5 to 1.5
    uint32_t minBlink = (uint32_t)(4000 * blinkMod);
    uint32_t maxBlink = (uint32_t)(8000 * blinkMod);
    
    // Check if we should blink (single frame blink)
    if (now - lastBlinkTime > blinkInterval) {
        isBlinking = true;
        lastBlinkTime = now;
        blinkInterval = random(minBlink, maxBlink);
    }

    // Calculate intensity-adjusted intervals
    // Excited pig looks around more, sad pig stares
    // NOTE: Reduced mood effect (was /150, now /300) to prevent frantic movement at high happiness
    float flipMod = 1.0f - (moodIntensity / 300.0f);  // ~0.66 to ~1.33
    uint32_t minWalk = (uint32_t)(30000 * flipMod);   // Walk timer: 30s base
    uint32_t maxWalk = (uint32_t)(75000 * flipMod);   // Walk timer: 75s base
    uint32_t minLook = (uint32_t)(4000 * flipMod);    // Look timer: 4s base (more frequent looks)
    uint32_t maxLook = (uint32_t)(15000 * flipMod);   // Look timer: 15s base
    
    // === ORGANIC RANDOM BEHAVIORS ===
    // Disable all movement while grass is moving (treadmill mode)
    if (!transitioning && !grassMoving && !pendingGrassStart) {
        
        // --- LOOK BEHAVIOR: Random glances with personality ---
        if (now - lastLookTime > lookInterval) {
            int lookRoll = random(0, 100);
            
            if (lookRoll < 35) {
                // 35%: Simple head turn
                facingRight = !facingRight;
            } else if (lookRoll < 55) {
                // 20%: Look one way, then back (curious double-take)
                facingRight = !facingRight;
                // Schedule a quick look-back by shortening next interval
                lookInterval = random(800, 1500);  // Quick follow-up
                lastLookTime = now;
                goto skip_look_reset;  // Don't reset with normal interval
            } else if (lookRoll < 70) {
                // 15%: Sniff while looking (something caught attention)
                facingRight = random(0, 2) == 0;  // Random direction
                sniff();
            } else if (lookRoll < 82) {
                // 12%: Ear wiggle (alert/listening)
                wiggleEars();
            } else if (lookRoll < 90) {
                // 8%: Blink slowly (relaxed)
                blink();
            }
            // 10%: Do nothing (just chill)
            
            lastLookTime = now;
            // Vary the interval more - sometimes rapid, sometimes long pauses
            if (random(0, 5) == 0) {
                lookInterval = random(1500, 4000);  // 20% chance: quick succession
            } else {
                lookInterval = random(minLook, maxLook);
            }
        }
        skip_look_reset:
        
        // --- WALK BEHAVIOR: Always stay at LEFT/RIGHT edges for bubble positioning ---
        // Pig should ALWAYS be at edges where bubble can float beside, never in center
        if (now - lastFlipTime > flipInterval) {
            int walkRoll = random(0, 100);
            int targetX;
            
            // Define edge zones (bubble floats beside pig, not above)
            const int LEFT_EDGE = 20;   // Left rest position
            const int RIGHT_EDGE = 108; // Right rest position
            
            if (walkRoll < 50) {
                // 50%: Walk to opposite edge (primary behavior)
                targetX = onRightSide ? LEFT_EDGE : RIGHT_EDGE;
            } else if (walkRoll < 85) {
                // 35%: Walk to random edge (left or right)
                targetX = random(0, 2) == 0 ? LEFT_EDGE : RIGHT_EDGE;
            } else if (walkRoll < 95) {
                // 10%: Short shuffle within current edge zone
                if (onRightSide) {
                    targetX = random(85, 108);  // Stay in right zone
                } else {
                    targetX = random(20, 45);   // Stay in left zone
                }
            } else {
                // 5%: Stay put, just turn around (fake walk)
                facingRight = !facingRight;
                lastFlipTime = now;
                flipInterval = random(minWalk / 2, maxWalk / 2);
                goto skip_walk;
            }
            
            // Only walk if destination is different enough (avoid micro-movements)
            if (abs(targetX - currentX) > 15) {
                bool goingRight = targetX > currentX;
                
                transitioning = true;
                transitionStartTime = now;
                transitionFromX = currentX;
                transitionToX = targetX;
                transitionToFacingRight = goingRight;  // Face direction of travel
                
                lastFlipTime = now;
                // Vary wait time more randomly
                if (random(0, 4) == 0) {
                    flipInterval = random(15000, 30000);  // 25% chance: shorter wait
                } else {
                    flipInterval = random(minWalk, maxWalk);
                }
            } else {
                // Destination too close, just look that way instead
                facingRight = targetX > currentX;
                lastFlipTime = now;
                flipInterval = random(minWalk / 3, minWalk);
            }
        }
        skip_walk:;
    }
    
    // Select frame based on state and direction (blink modifies eye only, not ears)
    const char** frame;
    bool shouldBlink = isBlinking && currentState != AvatarState::SLEEPY;
    
    // Clear blink flag after reading (single frame blink)
    if (isBlinking) {
        isBlinking = false;
    }
    
    switch (currentState) {
        case AvatarState::HAPPY:    
            frame = facingRight ? AVATAR_HAPPY_R : AVATAR_HAPPY_L; break;
        case AvatarState::EXCITED:  
            frame = facingRight ? AVATAR_EXCITED_R : AVATAR_EXCITED_L; break;
        case AvatarState::HUNTING:  
            frame = facingRight ? AVATAR_HUNTING_R : AVATAR_HUNTING_L; break;
        case AvatarState::SLEEPY:   
            frame = facingRight ? AVATAR_SLEEPY_R : AVATAR_SLEEPY_L; break;
        case AvatarState::SAD:      
            frame = facingRight ? AVATAR_SAD_R : AVATAR_SAD_L; break;
        case AvatarState::ANGRY:    
            frame = facingRight ? AVATAR_ANGRY_R : AVATAR_ANGRY_L; break;
        default:                    
            frame = facingRight ? AVATAR_NEUTRAL_R : AVATAR_NEUTRAL_L; break;
    }
    
    drawFrame(canvas, frame, 3, shouldBlink, facingRight, isSniffing);
}

void Avatar::drawFrame(M5Canvas& canvas, const char** frame, uint8_t lines, bool blink, bool faceRight, bool sniff) {
    // Star system background layer (behind pig)
    updateStars();
    drawStars(canvas);
    drawTree(canvas);   // Fruit tree — behind the pig, on the grass line
    fillPigBoundingBox(canvas);

    canvas.setTextDatum(top_left);
    canvas.setTextSize(3);
    canvas.setTextColor(realActive() ? REAL_PIG : getDrawColor());  // pink pig in Realistic

    uint32_t now = millis();
    
    // Watchdog: if caller stops refreshing attack shake, auto-disable after 250ms
    if (attackShakeRefreshTime == 0 || (now - attackShakeRefreshTime) > 250) {
        attackShakeActive = false;
        attackShakeStrong = false;
    }
    
    // Handle cute jump timeout
    if (jumpActive && (now - jumpStartTime > JUMP_DURATION_MS)) {
        jumpActive = false;
    }
    
    // === Attack hop: slide currentX along the pre-computed hop targets ===
    if (attackHopActive) {
        uint32_t hopElapsed = now - attackHopStartTime;
        uint32_t totalHopTime = (uint32_t)attackHopTotal * ATTACK_HOP_MS;
        if (hopElapsed >= totalHopTime) {
            attackHopActive = false;
            currentX = attackHopOriginX;
            if (grassMoving) facingRight = !grassDirection;
        } else {
            uint8_t hopIdx = hopElapsed / ATTACK_HOP_MS;
            if (hopIdx >= attackHopTotal) hopIdx = attackHopTotal - 1;
            attackHopIndex = hopIdx;
            float hopT = (float)(hopElapsed - hopIdx * ATTACK_HOP_MS) / (float)ATTACK_HOP_MS;
            float smoothT = hopT * hopT * (3.0f - 2.0f * hopT);
            int16_t fromX = (hopIdx == 0) ? attackHopOriginX : attackHopTargets[hopIdx - 1];
            int16_t toX = attackHopTargets[hopIdx];
            currentX = fromX + (int)((toX - fromX) * smoothT);
            facingRight = (toX > fromX);
        }
    }

    // Calculate vertical shake/jump offset
    int shakeY = 0;
    if (attackHopActive) {
        uint32_t hopElapsed = now - attackHopStartTime;
        uint32_t hopLocal = hopElapsed - (uint32_t)attackHopIndex * ATTACK_HOP_MS;
        float t = (float)hopLocal / (float)ATTACK_HOP_MS;
        float arc = 4.0f * t * (1.0f - t);
        shakeY = -(int)(arc * ATTACK_HOP_HEIGHT);
    } else if (jumpActive) {
        uint32_t elapsed = now - jumpStartTime;
        float t = (float)elapsed / (float)JUMP_DURATION_MS;
        float arc = 4.0f * t * (1.0f - t);
        shakeY = -(int)(arc * JUMP_HEIGHT);  // Negative = up
    } else if (attackShakeActive) {
        const int amp = attackShakeStrong ? 6 : 4;
        shakeY = (esp_random() % 2 == 0) ? amp : -amp;
    } else if (spinActive) {
        uint32_t elapsed = now - spinStart;
        if (elapsed >= SPIN_DURATION_MS) { spinActive = false; }
        else {
            float t = (float)elapsed / (float)SPIN_DURATION_MS;
            shakeY = -(int)(4.0f * t * (1.0f - t) * JUMP_HEIGHT);
            uint8_t flipPhase = elapsed / (SPIN_DURATION_MS / SPIN_FLIPS);
            facingRight = (flipPhase % 2 == 0);
        }
    } else if (perkUpActive) {
        uint32_t elapsed = now - perkUpStart;
        if (elapsed >= PERK_UP_DURATION_MS) { perkUpActive = false; }
        else { float t = (float)elapsed / (float)PERK_UP_DURATION_MS;
               shakeY = -(int)(4.0f * t * (1.0f - t) * PERK_UP_HEIGHT); }
    } else if (flinchActive) {
        uint32_t elapsed = now - flinchStart;
        if (elapsed >= FLINCH_DURATION_MS) { flinchActive = false; }
        else if (elapsed < 150) shakeY = 3;                      // duck down
        else shakeY = (esp_random() % 2 == 0) ? 2 : -2;          // jitter
    } else if (pawScratchActive) {
        if (now - pawScratchStart >= PAW_SCRATCH_DURATION_MS) pawScratchActive = false;
        // no Y offset — X oscillation handled below
    } else if (treeColliding) {
        shakeY = ((now / 40) % 3 == 0) ? -2 : ((now / 40) % 3 == 1) ? 2 : 0;
    } else if (transitioning || grassMoving) {
        static const int bouncePattern[4] = {0, -3, -1, -2};
        int phase = (now / 80) % 4;
        shakeY = bouncePattern[phase];
    } else {
        // Idle breathing: triangle wave, 3s period, 0 to -2px (lift only)
        uint32_t breathePhase = now % 3000;
        shakeY = (breathePhase < 1500) ? -(int)(breathePhase * 2 / 1500)
                                       : -(int)((3000 - breathePhase) * 2 / 1500);
    }

    // Use animated currentX position (set during transition or at rest)
    int startX = currentX;
    if (pawScratchActive) {
        uint32_t elapsed = now - pawScratchStart;
        startX += ((elapsed / 100) % 2 == 0) ? 2 : -2;   // paw scratch X oscillation
    }
    if (treeColliding) startX += ((now / 50) % 2 == 0) ? PX : -PX;  // bonk into trunk
    int startY = 40 + shakeY;  // pig feet align with the grass ground (baseY=106)
    int lineHeight = 22;
    
    for (uint8_t i = 0; i < lines; i++) {
        // Handle body line (i=2) for dynamic tail
        if (i == 2) {
            char bodyLine[16];
            bool tailOnLeft = false;  // Track if tail prefix added (needs X offset)
            if (grassMoving || pendingGrassStart) {
                // Treadmill mode: always show tail
                if (faceRight) {
                    strncpy(bodyLine, "z(    )", sizeof(bodyLine));  // Tail on left when facing right
                    tailOnLeft = true;
                } else {
                    strncpy(bodyLine, "(    )z", sizeof(bodyLine));  // Tail on right when facing left
                }
            } else if (transitioning) {
                // During transition: show tail on trailing side
                bool movingRight = (transitionToX > transitionFromX);
                if (movingRight) {
                    strncpy(bodyLine, "z(    )", sizeof(bodyLine));  // Tail trails on left
                    tailOnLeft = true;
                } else {
                    strncpy(bodyLine, "(    )z", sizeof(bodyLine));  // Tail trails on right
                }
            } else {
                // Stationary: static 'z' tail, burst wiggle (z/~) on celebrations
                char tail = 'z';
                if (tailWiggleActive) {
                    if ((now - tailWiggleStart) < TAIL_WIGGLE_DURATION_MS) {
                        tail = ((now / 120) % 2 == 0) ? 'z' : '~';  // ~4 waggles/sec
                    } else {
                        tailWiggleActive = false;
                    }
                }
                if (faceRight) {
                    snprintf(bodyLine, sizeof(bodyLine), "%c(    )", tail);  // tail on left
                    tailOnLeft = true;
                } else {
                    snprintf(bodyLine, sizeof(bodyLine), "(    )%c", tail);  // tail on right
                }
            }
            bodyLine[sizeof(bodyLine) - 1] = '\0';
            // When tail is on left (z prefix), offset X back by 1 char width (18px at size 3)
            // to keep body aligned with head
            int bodyX = tailOnLeft ? (startX - 18) : startX;
            canvas.drawString(bodyLine, bodyX, startY + i * lineHeight);
        } else if (i == 1 && (blink || sniff)) {
            // Face line - modify eye and/or nose
            // Face format: "(X 00)" for right-facing, "(00 X)" for left-facing
            char modifiedLine[16];
            strncpy(modifiedLine, frame[i], sizeof(modifiedLine) - 1);
            modifiedLine[sizeof(modifiedLine) - 1] = '\0';
            
            if (blink) {
                // Replace eye character with '-' for blink
                if (faceRight) {
                    modifiedLine[1] = '-';  // Eye position in "(X 00)"
                } else {
                    modifiedLine[4] = '-';  // Eye position in "(00 X)"
                }
            }
            
            if (sniff) {
                // Animated sniff - cycle through nose shapes
                char n1, n2;
                switch (sniffFrame) {
                    case 0: n1 = 'o'; n2 = 'o'; break;  // oo
                    case 1: n1 = 'o'; n2 = 'O'; break;  // oO
                    case 2: n1 = 'O'; n2 = 'o'; break;  // Oo
                    default: n1 = 'o'; n2 = 'o'; break;
                }
                // Nose is at positions 3-4 for right-facing "(X 00)"
                // Nose is at positions 1-2 for left-facing "(00 X)"
                if (faceRight) {
                    modifiedLine[3] = n1;
                    modifiedLine[4] = n2;
                } else {
                    modifiedLine[1] = n1;
                    modifiedLine[2] = n2;
                }
            }
            
            canvas.drawString(modifiedLine, startX, startY + i * lineHeight);
        } else {
            canvas.drawString(frame[i], startX, startY + i * lineHeight);
        }
    }
    
    // Draw grass below piglet
    drawGrass(canvas);

    // Celebration sparkles on top of everything
    updateAndDrawSparkles(canvas);
}

void Avatar::setGrassMoving(bool moving, bool directionRight) {
    // Early exit if already in requested state (prevents per-frame overhead)
    if (moving && (grassMoving || pendingGrassStart)) {
        return;  // Already moving or pending - don't interrupt
    }
    if (!moving && !grassMoving && !pendingGrassStart) {
        return;  // Already stopped
    }
    
    if (moving) {
        // COOLDOWN CHECK: Don't start grass if we just stopped
        // This prevents rapid on/off/on/off state changes from causing macarena
        uint32_t now = millis();
        if (lastGrassStopTime > 0 && (now - lastGrassStopTime) < GRASS_REST_COOLDOWN_MS) {
            return;  // Still in cooldown period - pig needs rest
        }
        
        grassDirection = directionRight;
        
        // Calculate correct treadmill position based on direction
        // Grass RIGHT: pig at X=108 (tail margin on right)
        // Grass LEFT: pig at X=20 (tail margin on left: 20-18=2)
        int targetX = directionRight ? 108 : 20;
        
        if (transitioning) {
            // Check if this is a coast-back transition (pig returning to rest at X=20)
            // Don't interrupt coast-back with grass start - let the pig chill first
            // This prevents the "macarena" bug where rapid state changes cause endless back-and-forth
            if (transitionToX == 20) {
                return;  // Coast-back in progress - pig needs a break
            }
            // Already sliding to grass position - queue grass
            pendingGrassStart = true;
            grassMoving = false;
        } else if (currentX != targetX) {
            // Not at correct treadmill position - slide there first
            startWindupSlide(targetX, directionRight);  // face direction of travel
            pendingGrassStart = true;
            grassMoving = false;
        } else {
            // Already at correct position - start grass immediately
            facingRight = !directionRight;
            grassMoving = true;
            pendingGrassStart = false;
        }
        
        // Clear cooldown since we successfully started
        lastGrassStopTime = 0;
    } else {
        // Stop grass and coast back to resting position
        grassMoving = false;
        pendingGrassStart = false;
        
        // Start cooldown timer - pig needs rest before grass can start again
        lastGrassStopTime = millis();
        
        // Reset walk timer to prevent immediate post-coast walk trigger
        lastFlipTime = millis();
        
        // Coast back to left resting position (X=20 for tail margin)
        startWindupSlide(20, false);  // X=20, face left when done
    }
}

void Avatar::setGrassSpeed(uint16_t ms) {
    grassSpeed = ms;
}

void Avatar::setGrassPattern(const char*) {
    // No-op: pixel-blade grass has no character pattern (kept for API compat).
}

void Avatar::resetGrassPattern() {
    grassOffset = 0;
    for (int i = 0; i < GRASS_BLADE_MAX; i++) {
        grassBlades[i].height = random(6, 20);
        grassBlades[i].lean   = random(-3, 4);
        grassBlades[i].width  = random(1, 4);
    }
}

void Avatar::updateGrass() {
    if (!grassMoving) return;

    const int count    = DISPLAY_W / GRASS_STRIDE;   // 30 (V8) / 40 (Pancake)
    const int16_t span = DISPLAY_W + 60;             // tree-offset wrap span

    uint32_t now = millis();
    // Pixel-level scroll: shift 1px every grassSpeed/STRIDE ms (smooth).
    uint32_t pixelInterval = grassSpeed / GRASS_STRIDE;
    if (pixelInterval < 1) pixelInterval = 1;
    if (now - lastGrassUpdate < pixelInterval) return;
    lastGrassUpdate = now;

    // The tree is part of the world: it scrolls 1px per grass pixel (original).
    if (grassDirection) {                 // world scrolls right
        grassOffset++;
        if (treePhase != TreePhase::HIDDEN) treeScrollOffset++;
        if (grassOffset >= GRASS_STRIDE) {
            grassOffset = 0;
            GrassBlade last = grassBlades[count - 1];
            for (int i = count - 1; i > 0; i--) grassBlades[i] = grassBlades[i - 1];
            grassBlades[0] = last;
            if (treeScrollOffset > span) treeScrollOffset -= span;
        }
    } else {                              // world scrolls left
        grassOffset--;
        if (treePhase != TreePhase::HIDDEN) treeScrollOffset--;
        if (grassOffset < 0) {
            grassOffset = GRASS_STRIDE - 1;
            GrassBlade first = grassBlades[0];
            for (int i = 0; i < count - 1; i++) grassBlades[i] = grassBlades[i + 1];
            grassBlades[count - 1] = first;
            if (treeScrollOffset < -span) treeScrollOffset += span;
        }
    }

    // ~3% chance: re-randomize one blade for organic variety.
    if (random(0, 30) == 0) {
        int idx = random(0, count);
        grassBlades[idx].height = random(6, 20);
        grassBlades[idx].lean   = random(-3, 4);
        grassBlades[idx].width  = random(1, 4);
    }
}

void Avatar::drawGrass(M5Canvas& canvas) {
    updateGrass();

    uint32_t now = millis();
    uint16_t color = realActive() ? REAL_GRASS : getDrawColor();  // green in Realistic
    const int16_t baseY = 106;                 // ground line (matches the tree)
    const int count  = DISPLAY_W / GRASS_STRIDE;
    const int center = DISPLAY_W / 2;

    // Shake state (screen-shake ripple through the blades)
    bool  shakeActive    = Display::isShaking();
    float shakeDecay     = shakeActive ? Display::getShakeDecay() : 0.0f;
    uint8_t shakeIntensity = shakeActive ? Display::getShakeIntensity() : 0;

    // Solid ground line
    canvas.fillRect(0, snapPx(baseY - 1), DISPLAY_W, PX, color);

    // Pig footprint (for grass bending) — ~108px pig at currentX.
    // (Always on the ground until the jump/attack-hop animations land in stage 3.)
    bool pigOnGround = true;
    int pigLeft   = currentX + 9;
    int pigRight  = currentX + 99;
    int pigCenter = (pigLeft + pigRight) / 2;
    int pigHalf   = (pigRight - pigLeft) / 2;

    // Tree screen X (for the collision ripple)
    const int16_t WRAP_HI = DISPLAY_W + 20, WRAP_LO = -80, WRAP_SPAN = WRAP_HI - WRAP_LO;
    int16_t treeScreenX = treeTrunk.baseX + treeScrollOffset;
    while (treeScreenX > WRAP_HI) treeScreenX -= WRAP_SPAN;
    while (treeScreenX < WRAP_LO) treeScreenX += WRAP_SPAN;

    for (int i = 0; i < count; i++) {
        int16_t cx = i * GRASS_STRIDE + grassOffset;
        if (cx < -GRASS_STRIDE) cx += DISPLAY_W + GRASS_STRIDE;
        if (cx >= DISPLAY_W) continue;

        const GrassBlade& b = grassBlades[i];
        int16_t drawHeight = b.height;
        int8_t  drawLean   = b.lean;

        // Ambient wind sway — triangle wave, ~2.5s period, per-blade phase
        {
            uint32_t phase = now + (uint32_t)i * 197;
            int wave = (int)(phase % 2500);
            int sway = (wave < 1250) ? (wave - 625) : (1875 - wave);
            drawLean += (int8_t)(sway * PX / 625);
        }

        // Bend grass under the pig body
        if (pigOnGround && cx >= pigLeft && cx <= pigRight) {
            int d = cx - pigCenter; if (d < 0) d = -d;
            float bend = 1.0f - (float)d / (float)pigHalf;
            drawHeight = b.height - (int16_t)((float)b.height * 0.7f * bend);
            if (drawHeight < PX) drawHeight = PX;
            int8_t leanPush = (int8_t)(4.0f * bend);
            drawLean = (cx < pigCenter) ? (b.lean - leanPush) : (b.lean + leanPush);
        }

        // Shake impact ripple (strongest at center, fades to edges)
        bool bladeInverted = false;
        if (shakeActive && shakeDecay > 0.05f) {
            float edgeDist = 1.0f - (float)(cx > center ? cx - center : center - cx) / (float)center;
            if (edgeDist < 0.0f) edgeDist = 0.0f;
            float impact = edgeDist * shakeDecay * ((float)shakeIntensity / 5.0f);
            if (impact > 0.15f) drawLean += ((now / 33) % 2 == 0) ? PX : -PX;
            if (impact > 0.5f)  bladeInverted = true;
        }

        // Tree-collision ripple from the trunk
        if (treeColliding) {
            int16_t dist = cx > treeScreenX ? cx - treeScreenX : treeScreenX - cx;
            int16_t radius = (int16_t)treeTrunk.crownRadius * 3;
            if (dist < radius) {
                float falloff = 1.0f - (float)dist / (float)radius;
                uint32_t phase = now + (uint32_t)(dist * 7);
                int8_t jitter = ((phase / 33) % 2 == 0) ? PX : -PX;
                drawLean += (int8_t)((float)jitter * falloff);
            }
        }

        int16_t tipX = snapPx(cx + drawLean);
        int16_t tipY = snapPx(baseY - drawHeight);
        uint16_t bladeColor = (bladeInverted || (pigOnGround && cx >= pigLeft && cx <= pigRight))
                              ? getBGColor() : color;
        fatLine(canvas, snapPx(cx), baseY, tipX, tipY, bladeColor);
    }

    // === Dust trail particles (kicked up by the running pig) ===
    bool isRunning = transitioning || grassMoving;
    if (isRunning && pigOnGround && now - lastTrailSpawn > 70) {
        lastTrailSpawn = now;
        TrailParticle& p = trailParticles[trailSpawnIdx];
        trailSpawnIdx = (trailSpawnIdx + 1) % TRAIL_COUNT;
        if (facingRight) {
            p.x = (float)(currentX + random(0, 20));
            p.vx = -(1.0f + (float)random(0, 20) / 10.0f);
        } else {
            p.x = (float)(currentX + 88 + random(0, 20));
            p.vx = 1.0f + (float)random(0, 20) / 10.0f;
        }
        p.y = (float)(96 + random(0, 10));
        p.vy = -(0.2f + (float)random(0, 10) / 20.0f);
        p.startX = p.x;
        p.maxDist = 30.0f + (float)random(0, 31);
        p.baseSize = random(1, 3);
        p.active = true;
    }
    if (now - lastTrailUpdate > 50) {
        lastTrailUpdate = now;
        for (int i = 0; i < TRAIL_COUNT; i++) {
            if (!trailParticles[i].active) continue;
            trailParticles[i].x += trailParticles[i].vx;
            trailParticles[i].y += trailParticles[i].vy;
            float dx = trailParticles[i].x - trailParticles[i].startX;
            if (dx < 0) dx = -dx;
            if (dx >= trailParticles[i].maxDist) trailParticles[i].active = false;
        }
    }
    for (int i = 0; i < TRAIL_COUNT; i++) {
        if (!trailParticles[i].active) continue;
        int tpx = snapPx((int16_t)trailParticles[i].x);
        int tpy = snapPx((int16_t)trailParticles[i].y);
        if (tpx < 0 || tpx >= DISPLAY_W) continue;
        canvas.fillRect(tpx, tpy, PX, PX, color);
    }
}

// --- Night sky star system ---
bool Avatar::isNightTime() {
    uint32_t now = millis();

    // Cache rtc reads, check every 60 seconds
    if (now - lastNightCheck < 60000 && lastNightCheck != 0) {
        return cachedNightMode;
    }
    lastNightCheck = now;

    auto dt = M5.Rtc.getDateTime();
    if (dt.date.year >= 2024) {
        uint8_t hour = dt.time.hours;
        cachedNightMode = (hour >= 20 || hour < 6);
        return cachedNightMode;
    }

    time_t unixNow = time(nullptr);
    if (unixNow >= 1700000000) {
        struct tm timeinfo;
        localtime_r(&unixNow, &timeinfo);
        uint8_t hour = (uint8_t)timeinfo.tm_hour;
        cachedNightMode = (hour >= 20 || hour < 6);
        return cachedNightMode;
    }

    cachedNightMode = false;
    return false;
}

bool Avatar::areStarsActive() {
    return starsActive;
}

void Avatar::initStarPositions() {
    // Pre-gen star positions, hide until spawn
    for (uint8_t i = 0; i < MAX_STARS; i++) {
        // y 20-100 sky/backdrop, bubble still wins
        // x 5-235 near full width
        stars[i].x = random(5, 235);
        // Match rain clip: keep stars above grass (rain clips at y < 88)
        stars[i].y = random(20, 88);
        stars[i].size = 1;
        stars[i].brightness = 0;
        stars[i].fadeInStart = 0;
        // About 20 percent twinkle
        stars[i].isBlinking = (random(0, 100) < 20);
    }
}

void Avatar::updateStars() {
    uint32_t now = millis();

    // Never show stars while raining
    if (Weather::isRaining()) {
        if (starsActive) {
            starsActive = false;
            starCount = 0;
        }
        return;
    }

    // Night mode transition
    bool nightNow = isNightTime();

    if (nightNow && !starsActive) {
        // Night starting, spawn sequence online
        starsActive = true;
        starCount = 0;
        lastStarSpawn = now;
        nextSpawnDelay = random(800, 4001);  // 800ms to 4s
        initStarPositions();
    } else if (!nightNow && starsActive) {
        // Day starting, kill stars
        starsActive = false;
        starCount = 0;
    }

    if (!starsActive) return;

    // Spawn new star when timer expires
    if (starCount < MAX_STARS && (now - lastStarSpawn >= nextSpawnDelay)) {
        stars[starCount].fadeInStart = now;
        stars[starCount].brightness = 0;
        starCount++;
        lastStarSpawn = now;
        nextSpawnDelay = random(800, 4001);
    }

    // Update visible stars, fade-in, twinkle handled in draw
    for (uint8_t i = 0; i < starCount; i++) {
        uint32_t age = now - stars[i].fadeInStart;
        if (age < 500) {
            stars[i].brightness = (age * 255) / 500;
        } else {
            stars[i].brightness = 255;
        }
    }
}

void Avatar::fillPigBoundingBox(M5Canvas& canvas) {
    if (!starsActive || starCount == 0) return;

    int boxX = currentX - 25;
    int boxW = 155;  // covers tail + 7 chars + margin
    int boxY = 28;   // base y (40) minus jump headroom (12)
    int boxH = 84;   // down to the grass line (~112)

    // Clamp to screen
    if (boxX < 0) { boxW += boxX; boxX = 0; }
    if (boxX + boxW > DISPLAY_W) boxW = DISPLAY_W - boxX;

    canvas.fillRect(boxX, boxY, boxW, boxH, getBGColor());
}

void Avatar::drawStars(M5Canvas& canvas) {
    if (!starsActive || starCount == 0) return;

    uint32_t now = millis();
    uint16_t fg = realActive() ? REAL_STAR : getDrawColor();  // warm starlight in Realistic
    canvas.setTextSize(1);
    canvas.setTextColor(fg);
    canvas.setTextDatum(top_left);

    for (uint8_t i = 0; i < starCount; i++) {
        if (stars[i].brightness < 128) continue;
        if (stars[i].y >= 88) continue;  // Match rain clip above grass

        char starChar = '.';
        if (stars[i].isBlinking) {
            uint32_t phase = (now + i * 700) % 4000;
            if (phase >= 1700 && phase < 2300) {
                starChar = '*';
            }
        }
        canvas.drawChar(starChar, stars[i].x, stars[i].y);
    }
}

// --- Phase 8: Direction control helpers ---
void Avatar::setFacingLeft() {
    facingRight = false;
}

void Avatar::setFacingRight() {
    facingRight = true;
}

// --- Phase 2: Attack shake control ---
void Avatar::setAttackShake(bool active, bool strong) {
    attackShakeActive = active;
    attackShakeStrong = strong;
    attackShakeRefreshTime = active ? millis() : 0;
}

// --- Thunder flash control (weather effect) ---
void Avatar::setThunderFlash(bool active) {
    thunderFlashActive = active;
}

bool Avatar::isThunderFlashing() {
    return thunderFlashActive;
}

// --- Phase 6: Windup slide for coast-back ---
void Avatar::startWindupSlide(int targetX, bool faceRight) {
    // Start a smooth transition to target position
    // Uses standard TRANSITION_DURATION_MS (300ms) from draw() logic
    if (currentX != targetX) {
        transitioning = true;
        transitionFromX = currentX;
        transitionToX = targetX;
        transitionStartTime = millis();
        transitionToFacingRight = faceRight;
    }
    // Set facing direction for when transition completes
    facingRight = faceRight;
}


// ==================================================================
//  Fruit-tree hunting animation — restored from commit e56ef5b and
//  adapted for Pancake. Self-contained fat-pixel tree rooted at the
//  grass line (baseY=106); horizontal already sized for 320px.
//  Collision (pig walks up & shakes it) is computed in Avatar::draw().
// ==================================================================
// Fruit tree state
TreePhase Avatar::treePhase = TreePhase::HIDDEN;
float Avatar::treeGrowth = 0.0f;
uint32_t Avatar::treeAnimStart = 0;
Avatar::TreeTrunk Avatar::treeTrunk = {};
Avatar::TreeBranch Avatar::treeBranches[MAX_BRANCHES] = {};
uint8_t Avatar::treeBranchCount = 0;
Avatar::TreeLeafCluster Avatar::treeLeaves[MAX_LEAF_CLUSTERS] = {};
uint8_t Avatar::treeLeafCount = 0;
uint8_t Avatar::treeEndpointLeafCount = 0;
Avatar::TreeFruit Avatar::treeFruits[MAX_TREE_FRUITS] = {};
uint8_t Avatar::treeFruitCount = 0;
uint32_t Avatar::treeSeed = 0;
bool Avatar::treePendingHide = false;
bool Avatar::treePendingShow = false;
uint8_t Avatar::treePendingFruits = 0;
uint32_t Avatar::treeAliveStart = 0;
int16_t Avatar::treeScrollOffset = 0;

// Tree-pig collision state (pig bumps into tree → both shake)
static bool treeColliding = false;
static int8_t treeCollisionShake = 0;  // rapid jitter applied to tree X

// Dropping fruit system (individual fruit falls on deauth success)
struct DroppingFruit {
    int16_t x, y;         // absolute screen position at detach
    uint8_t radius;
    uint32_t dropStart;   // millis() when detached
    bool active;
};
static constexpr uint8_t MAX_DROPPING = 4;
static DroppingFruit droppingFruits[MAX_DROPPING] = {{0}};

// Fruit splash particles (burst on ground impact)
struct FruitSplash {
    float x, y;
    float vx, vy;
    uint8_t size;       // 1-3px radius
    uint32_t spawnTime;
    bool active;
};
static constexpr uint8_t FRUIT_SPLASH_COUNT = 8;
static FruitSplash fruitSplashes[FRUIT_SPLASH_COUNT] = {{0}};
static uint8_t fruitSplashIdx = 0;

// Fat pixel size = font scale factor (text size 3 = 3x3 blocks)
static constexpr int16_t PX = 3;

static inline int16_t snapPx(int16_t v) {
    return (v >= 0) ? (v / PX) * PX : ((v - 2) / PX) * PX;
}

// Bresenham line on PX grid — stamps PX*PX blocks
static void fatLine(M5Canvas& canvas, int16_t x1, int16_t y1,
                    int16_t x2, int16_t y2, uint16_t color) {
    int gx1 = x1 / PX, gy1 = y1 / PX;
    int gx2 = x2 / PX, gy2 = y2 / PX;
    int dx = abs(gx2 - gx1), dy = abs(gy2 - gy1);
    int sx = (gx1 < gx2) ? 1 : -1, sy = (gy1 < gy2) ? 1 : -1;
    int err = dx - dy;
    while (true) {
        canvas.fillRect(gx1 * PX, gy1 * PX, PX, PX, color);
        if (gx1 == gx2 && gy1 == gy2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; gx1 += sx; }
        if (e2 < dx)  { err += dx; gy1 += sy; }
    }
}

static uint32_t treeLCG(uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return s;
}

// Fixed-point sin*256 for angles 0-180 in 15-degree steps (13 entries)
static const int16_t sin_lut[13] = {
    0, 66, 128, 181, 222, 248, 256, 248, 222, 181, 128, 66, 0
};

static int16_t lut_sin(uint8_t idx) {
    return (idx < 13) ? sin_lut[idx] : 0;
}

static int16_t lut_cos(uint8_t idx) {
    int8_t ci = 6 - (int8_t)idx;
    if (ci >= 0) return sin_lut[ci];
    return -sin_lut[-ci];
}


void Avatar::generateTree(uint8_t fruitCount) {
    treeSeed = esp_random();
    treeScrollOffset = 0;
    uint32_t s = treeSeed;

    // Position: opposite side from pig (screen-relative; at 320 == 33-72 / 248-287)
    if (onRightSide) {
        treeTrunk.baseX = 33 + (int16_t)(treeLCG(s) % 40);                 // left
    } else {
        treeTrunk.baseX = (DISPLAY_W - 72) + (int16_t)(treeLCG(s) % 40);   // near right edge
    }

    // Ensure minimum gap from pig
    int16_t gap = abs(treeTrunk.baseX - currentX);
    if (gap < 60) {
        treeTrunk.baseX = (currentX < DISPLAY_W / 2) ? (DISPLAY_W - 80) + (int16_t)(treeLCG(s) % 40)
                                                     : 33 + (int16_t)(treeLCG(s) % 40);
    }

    // Trunk dimensions — proportional to 107px canvas
    treeTrunk.trunkHeight = 66 + fruitCount + (uint8_t)(treeLCG(s) % 7);  // 67-81px
    treeTrunk.trunkWidth = 2 + (uint8_t)(treeLCG(s) % 2);     // 2-3 half-width
    treeTrunk.trunkLean = (int8_t)(treeLCG(s) % 7) - 3;       // -3..+3

    // Crown canopy — dominant visual element
    treeTrunk.crownRadius = 8 + treeTrunk.trunkHeight / 5 + (uint8_t)(treeLCG(s) % 3);

    // Trunk top (relative to baseX, baseY=106)
    int8_t topX = treeTrunk.trunkLean;
    int8_t topY = -(int8_t)treeTrunk.trunkHeight;

    // Always 3 main branches, fanned across 45-135 degree range (upward-biased)
    const uint8_t mainCount = 3;
    treeBranchCount = 0;
    treeLeafCount = 0;
    treeEndpointLeafCount = 0;
    uint8_t sector = 7 / mainCount;  // ~2 sectors across [3..9]

    // Branch origins distributed along trunk (50%, 70%, 100% of height)
    const uint8_t originPcts[3] = { 50, 70, 100 };

    // Track sub-branch endpoints for tertiary branching
    uint8_t subEndpoints[6];
    uint8_t subEndpointCount = 0;

    for (uint8_t m = 0; m < mainCount && treeBranchCount < MAX_BRANCHES; m++) {
        uint8_t sectorStart = 3 + m * sector;
        uint8_t angleIdx = sectorStart + (uint8_t)(treeLCG(s) % sector);
        if (angleIdx > 9) angleIdx = 9;

        int8_t originY = -(int8_t)(treeTrunk.trunkHeight * originPcts[m] / 100);
        int8_t originX = (int8_t)((int16_t)treeTrunk.trunkLean * originPcts[m] / 100);

        uint8_t length = 18 + (uint8_t)(treeLCG(s) % 15);  // 18-32

        int8_t dx = (int8_t)((int16_t)length * lut_cos(angleIdx) / 256);
        int8_t dy = (int8_t)(-(int16_t)length * lut_sin(angleIdx) / 256);

        TreeBranch& br = treeBranches[treeBranchCount];
        br.x1 = originX;
        br.y1 = originY;
        br.x2 = originX + dx;
        br.y2 = originY + dy;
        br.thickness = 2;
        treeBranchCount++;

        // Guaranteed sub-branch from 2/3 point of every main branch
        if (treeBranchCount < MAX_BRANCHES) {
            int8_t midX = br.x1 + (br.x2 - br.x1) * 2 / 3;
            int8_t midY = br.y1 + (br.y2 - br.y1) * 2 / 3;

            int8_t angleOff = 1 + (int8_t)(treeLCG(s) % 3);
            if (treeLCG(s) % 2 == 0) angleOff = -angleOff;
            int8_t subAngle = (int8_t)angleIdx + angleOff;
            if (subAngle < 3) subAngle = 3;
            if (subAngle > 9) subAngle = 9;

            uint8_t subLen = 12 + (uint8_t)(treeLCG(s) % 11);  // 12-22

            uint8_t subIdx = treeBranchCount;
            TreeBranch& sbr = treeBranches[subIdx];
            sbr.x1 = midX;
            sbr.y1 = midY;
            sbr.x2 = midX + (int8_t)((int16_t)subLen * lut_cos((uint8_t)subAngle) / 256);
            sbr.y2 = midY + (int8_t)(-(int16_t)subLen * lut_sin((uint8_t)subAngle) / 256);
            sbr.thickness = 1;
            treeBranchCount++;

            if (subEndpointCount < 6) {
                subEndpoints[subEndpointCount++] = subIdx;
            }
        }

        // Second sub-branch from 1/3 point of main branch
        if (treeBranchCount < MAX_BRANCHES) {
            int8_t thirdX = br.x1 + (br.x2 - br.x1) / 3;
            int8_t thirdY = br.y1 + (br.y2 - br.y1) / 3;

            int8_t angleOff2 = 1 + (int8_t)(treeLCG(s) % 3);
            if (treeLCG(s) % 2 == 0) angleOff2 = -angleOff2;
            int8_t subAngle2 = (int8_t)angleIdx + angleOff2;
            if (subAngle2 < 3) subAngle2 = 3;
            if (subAngle2 > 9) subAngle2 = 9;

            uint8_t subLen2 = 10 + (uint8_t)(treeLCG(s) % 9);  // 10-18

            uint8_t subIdx2 = treeBranchCount;
            TreeBranch& sbr2 = treeBranches[subIdx2];
            sbr2.x1 = thirdX;
            sbr2.y1 = thirdY;
            sbr2.x2 = thirdX + (int8_t)((int16_t)subLen2 * lut_cos((uint8_t)subAngle2) / 256);
            sbr2.y2 = thirdY + (int8_t)(-(int16_t)subLen2 * lut_sin((uint8_t)subAngle2) / 256);
            sbr2.thickness = 1;
            treeBranchCount++;

            if (subEndpointCount < 6) {
                subEndpoints[subEndpointCount++] = subIdx2;
            }
        }
    }

    // Tertiary branches from each sub-branch endpoint
    for (uint8_t t = 0; t < subEndpointCount && treeBranchCount < MAX_BRANCHES; t++) {
        const TreeBranch& parent = treeBranches[subEndpoints[t]];
        int8_t angleOff = 1 + (int8_t)(treeLCG(s) % 3);
        if (treeLCG(s) % 2 == 0) angleOff = -angleOff;

        int8_t terAngle = 6 + angleOff;
        if (terAngle < 3) terAngle = 3;
        if (terAngle > 9) terAngle = 9;

        uint8_t terLen = 8 + (uint8_t)(treeLCG(s) % 9);  // 8-16

        TreeBranch& tbr = treeBranches[treeBranchCount];
        tbr.x1 = parent.x2;
        tbr.y1 = parent.y2;
        tbr.x2 = parent.x2 + (int8_t)((int16_t)terLen * lut_cos((uint8_t)terAngle) / 256);
        tbr.y2 = parent.y2 + (int8_t)(-(int16_t)terLen * lut_sin((uint8_t)terAngle) / 256);
        tbr.thickness = 1;
        treeBranchCount++;
    }

    // Fruits anchored to branch endpoints directly
    treeFruitCount = fruitCount > MAX_TREE_FRUITS ? MAX_TREE_FRUITS : fruitCount;
    uint8_t fruitRadius = (treeFruitCount <= 4) ? 6 : 4;
    uint8_t endpointCount = treeBranchCount > 0 ? treeBranchCount : 1;

    for (uint8_t i = 0; i < treeFruitCount; i++) {
        uint8_t bi = (uint8_t)(treeLCG(s) % endpointCount);
        const TreeBranch& br = treeBranches[bi];
        int8_t scatter = 3;
        treeFruits[i].offsetX = br.x2 + (int8_t)((treeLCG(s) % (scatter * 2 + 1)) - scatter);
        treeFruits[i].offsetY = br.y2 + (int8_t)((treeLCG(s) % (scatter * 2 + 1)) - scatter);
        treeFruits[i].radius = fruitRadius;
        treeFruits[i].bobPhase = (uint8_t)(treeLCG(s) & 0xFF);
    }
}

void Avatar::showTree(uint8_t fruitCount) {
    if (fruitCount == 0) return;

    if (treePhase == TreePhase::COLLAPSING) {
        treePendingShow = true;
        treePendingFruits = fruitCount;
        return;
    }

    if (treePhase == TreePhase::GROWING) {
        return;
    }

    if (treePhase == TreePhase::ALIVE && treeFruitCount == fruitCount) {
        return;
    }

    if (treePhase == TreePhase::ALIVE) {
        // Update fruits in-place without re-growing
        uint32_t s = treeSeed + fruitCount;
        uint8_t fruitRadius = (fruitCount <= 4) ? 6 : 4;
        treeFruitCount = fruitCount > MAX_TREE_FRUITS ? MAX_TREE_FRUITS : fruitCount;
        uint8_t endpointCount = treeBranchCount > 0 ? treeBranchCount : 1;
        for (uint8_t i = 0; i < treeFruitCount; i++) {
            uint8_t bi = (uint8_t)(treeLCG(s) % endpointCount);
            const TreeBranch& br = treeBranches[bi];
            int8_t scatter = 3;
            treeFruits[i].offsetX = br.x2 + (int8_t)((treeLCG(s) % (scatter * 2 + 1)) - scatter);
            treeFruits[i].offsetY = br.y2 + (int8_t)((treeLCG(s) % (scatter * 2 + 1)) - scatter);
            treeFruits[i].radius = fruitRadius;
            treeFruits[i].bobPhase = (uint8_t)(treeLCG(s) & 0xFF);
        }
        return;
    }

    generateTree(fruitCount);
    treePhase = TreePhase::GROWING;
    treeGrowth = 0.0f;
    treeAnimStart = millis();
}

void Avatar::hideTree() {
    if (treePhase == TreePhase::HIDDEN || treePhase == TreePhase::COLLAPSING) return;

    if (treePhase == TreePhase::GROWING) {
        treePendingHide = true;
        return;
    }

    if (treePhase == TreePhase::ALIVE && (millis() - treeAliveStart < TREE_MIN_ALIVE_MS)) {
        treePendingHide = true;
        return;
    }

    treePhase = TreePhase::COLLAPSING;
    treeAnimStart = millis();
    treeGrowth = 1.0f;
}

bool Avatar::isTreeVisible() {
    return treePhase != TreePhase::HIDDEN;
}

void Avatar::dropFruit() {
    if (treeFruitCount == 0 || treePhase != TreePhase::ALIVE) return;

    uint8_t idx = treeFruitCount - 1;
    const TreeFruit& f = treeFruits[idx];

    const int16_t baseY = 106;
    const int16_t WRAP_HI = DISPLAY_W + 20, WRAP_LO = -80, WRAP_SPAN = WRAP_HI - WRAP_LO;
    int16_t bx = treeTrunk.baseX + treeScrollOffset;
    while (bx > WRAP_HI) bx -= WRAP_SPAN;    // screen-relative wrap
    while (bx < WRAP_LO) bx += WRAP_SPAN;

    // Ambient sway (match drawTree logic)
    int8_t sway = 0;
    uint32_t now = millis();
    int wave = (int)(now % 3000);
    sway = (wave < 1500) ? (int8_t)(((wave - 750) * PX) / 750)
                         : (int8_t)(((2250 - wave) * PX) / 750);

    for (uint8_t i = 0; i < MAX_DROPPING; i++) {
        if (!droppingFruits[i].active) {
            droppingFruits[i].x = bx + f.offsetX + sway;
            droppingFruits[i].y = baseY + f.offsetY;
            droppingFruits[i].radius = f.radius;
            droppingFruits[i].dropStart = now;
            droppingFruits[i].active = true;
            break;
        }
    }

    treeFruitCount--;

    if (treeFruitCount == 0) {
        hideTree();
    }
}

void Avatar::updateTree() {
    uint32_t now = millis();

    if (treePhase == TreePhase::ALIVE) {
        if (treePendingHide && (now - treeAliveStart >= TREE_MIN_ALIVE_MS)) {
            treePendingHide = false;
            treePhase = TreePhase::COLLAPSING;
            treeAnimStart = now;
            treeGrowth = 1.0f;
        }
        return;
    }

    if (treePhase == TreePhase::HIDDEN) return;

    uint32_t elapsed = now - treeAnimStart;

    if (treePhase == TreePhase::GROWING) {
        treeGrowth = (float)elapsed / (float)TREE_GROW_MS;
        if (treeGrowth >= 1.0f) {
            treeGrowth = 1.0f;
            if (treePendingHide) {
                treePendingHide = false;
                treePhase = TreePhase::COLLAPSING;
                treeAnimStart = now;
            } else {
                treePhase = TreePhase::ALIVE;
                treeAliveStart = now;
            }
        }
    } else if (treePhase == TreePhase::COLLAPSING) {
        treeGrowth = 1.0f - (float)elapsed / (float)TREE_COLLAPSE_MS;
        if (treeGrowth <= 0.0f) {
            treeGrowth = 0.0f;
            if (treePendingShow) {
                treePendingShow = false;
                generateTree(treePendingFruits);
                treePhase = TreePhase::GROWING;
                treeAnimStart = now;
            } else {
                treePhase = TreePhase::HIDDEN;
            }
        }
    }
}

// Fat-pixel fruit: large=5x5 diamond, small=3x3 cross
static void fatFruit(M5Canvas& canvas, int16_t cx, int16_t cy, int r,
                     uint16_t fill, uint16_t outline) {
    int16_t gx = snapPx(cx), gy = snapPx(cy);
    if (r >= 6) {
        // Large: 5x5 grid diamond — FG ring + BG center
        static const int8_t ring[][2] = {
            {-1,-2},{0,-2},{1,-2},
            {-2,-1},{2,-1},
            {-2, 0},{2, 0},
            {-2, 1},{2, 1},
            {-1, 2},{0, 2},{1, 2}
        };
        for (auto& p : ring)
            canvas.fillRect(gx + p[0]*PX, gy + p[1]*PX, PX, PX, outline);
        // Interior fill (3x3 center)
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
                canvas.fillRect(gx + dx*PX, gy + dy*PX, PX, PX, fill);
    } else {
        // Small: 3x3 cross — 4 FG cardinal + 1 BG center
        canvas.fillRect(gx, gy - PX, PX, PX, outline);
        canvas.fillRect(gx - PX, gy, PX, PX, outline);
        canvas.fillRect(gx + PX, gy, PX, PX, outline);
        canvas.fillRect(gx, gy + PX, PX, PX, outline);
        canvas.fillRect(gx, gy, PX, PX, fill);
    }
}

void Avatar::drawTree(M5Canvas& canvas) {
    updateTree();

    // World-scroll wrap bounds, screen-relative so the tree isn't parked
    // off-screen on the narrow V8 panel (320-wide Pancake: 340/-80/420).
    const int16_t WRAP_HI   = DISPLAY_W + 20;
    const int16_t WRAP_LO   = -80;
    const int16_t WRAP_SPAN = WRAP_HI - WRAP_LO;

    // The tree scrolls WITH the world — coupled 1px-per-grass-pixel in
    // updateGrass() (original behaviour), so it passes the pig from either side.

    // --- Pig-tree collision: when a tree scrolls onto the pig it shakes ---
    treeColliding = false;
    treeCollisionShake = 0;
    if (treePhase == TreePhase::ALIVE || treePhase == TreePhase::GROWING) {
        int16_t tbx = treeTrunk.baseX + treeScrollOffset;
        while (tbx > WRAP_HI) tbx -= WRAP_SPAN;
        while (tbx < WRAP_LO) tbx += WRAP_SPAN;
        int16_t treeLeft  = tbx - treeTrunk.crownRadius;
        int16_t treeRight = tbx + treeTrunk.crownRadius;
        int16_t pigL = currentX + 18;
        int16_t pigR = currentX + 90;
        if (pigR > treeLeft && pigL < treeRight) {
            treeColliding = true;
            treeCollisionShake = ((millis() / 33) % 2 == 0) ? PX : -PX;
        }
    }

    // Check if any dropping fruits are still active
    bool hasDropping = false;
    for (uint8_t i = 0; i < MAX_DROPPING; i++) {
        if (droppingFruits[i].active) { hasDropping = true; break; }
    }

    if (treePhase == TreePhase::HIDDEN && !hasDropping) return;

    uint16_t fg = getDrawColor();
    uint16_t bg = getBGColor();
    // Realistic palette: brown trunk, green crown, red fruit. Falls back to the
    // mono fg/bg so every other theme is unchanged.
    bool     real       = realActive();
    uint16_t trunkCol   = real ? REAL_TRUNK         : fg;
    uint16_t leafCol    = real ? REAL_LEAF          : fg;   // crown/branches
    uint16_t fruitFill  = real ? REAL_FRUIT         : bg;
    uint16_t fruitLine  = real ? REAL_FRUIT_OUTLINE : fg;
    uint16_t splashCol  = real ? REAL_FRUIT         : fg;
    uint32_t now = millis();

    const int16_t baseY = 106;
    int16_t bx = treeTrunk.baseX + treeScrollOffset;
    // Wrap to screen bounds (screen-relative)
    while (bx > WRAP_HI) bx -= WRAP_SPAN;
    while (bx < WRAP_LO) bx += WRAP_SPAN;

    // Ambient sway when alive: ±1 fat pixel, 3s period triangle wave
    int8_t sway = 0;
    if (treePhase == TreePhase::ALIVE) {
        int wave = (int)(now % 3000);
        sway = (wave < 1500) ? (int8_t)(((wave - 750) * PX) / 750)
                             : (int8_t)(((2250 - wave) * PX) / 750);
    }

    // Tree-pig collision: add rapid jitter on top of ambient sway
    if (treeColliding) {
        sway += treeCollisionShake;
    }

    // Sand-drop collapse: collapseT goes 0→1 as tree falls
    bool collapsing = (treePhase == TreePhase::COLLAPSING);
    float collapseT = collapsing ? (1.0f - treeGrowth) : 0.0f;
    float collapseT2 = collapseT * collapseT;

    // --- Phase 1: Trunk (growth 0.0 - 0.25) ---
    float trunkProgress = 1.0f;
    if (!collapsing && treeGrowth < 0.25f) {
        trunkProgress = treeGrowth / 0.25f;
    }

    if (trunkProgress > 0.0f) {
        int16_t trunkH = (int16_t)((float)treeTrunk.trunkHeight * trunkProgress);
        int8_t lean = treeTrunk.trunkLean + sway;

        int16_t trunkTopDrop = 0;
        if (collapsing) {
            trunkTopDrop = (int16_t)(collapseT2 * (float)treeTrunk.trunkHeight);
            trunkH -= trunkTopDrop;
            if (trunkH <= 0) trunkH = 0;
        }

        if (trunkH > 0) {
            int16_t trunkTop = baseY - trunkH;

            for (int16_t row = 0; row < trunkH; row += PX) {
                float t = (float)row / (float)(trunkH > 1 ? trunkH - 1 : 1);
                int16_t rowLean = snapPx(lean - (int16_t)((float)lean * t));
                int hwFat = 1 + (int)(t * 1.0f + 0.5f);
                int16_t h = (row + PX > trunkH) ? (trunkH - row) : PX;
                for (int dx = -hwFat; dx <= hwFat; dx++) {
                    canvas.fillRect(snapPx(bx + rowLean) + dx * PX, trunkTop + row, PX, h, trunkCol);
                }
            }
        }
    }

    // --- Phase 2: Crown + Branches + Leaves (growth 0.25 - 0.75) ---
    if (collapsing || treeGrowth >= 0.25f) {

    float branchProgress = 1.0f;
    if (!collapsing) {
        branchProgress = (treeGrowth - 0.25f) / 0.5f;
        if (branchProgress > 1.0f) branchProgress = 1.0f;
    }

    for (uint8_t i = 0; i < treeBranchCount; i++) {
        const TreeBranch& br = treeBranches[i];
        int16_t sx = bx + br.x1 + sway;
        int16_t sy = baseY + br.y1;
        int16_t fullEx = bx + br.x2 + sway;
        int16_t fullEy = baseY + br.y2;

        if (collapsing) {
            int16_t distFromGround = baseY - fullEy;
            if (distFromGround < 0) distFromGround = 0;
            int16_t dropY = (int16_t)(collapseT2 * (float)distFromGround);
            fullEy += dropY;
            sy += (int16_t)(collapseT2 * (float)(baseY - sy) * 0.3f);
            if (fullEy > baseY || sy > baseY) continue;
        }

        int16_t ex = sx + (int16_t)((float)(fullEx - sx) * branchProgress);
        int16_t ey = sy + (int16_t)((float)(fullEy - sy) * branchProgress);

        fatLine(canvas, sx, sy, ex, ey, leafCol);
    }

    // --- Phase 3: Fruits (growth 0.75 - 1.0) ---
    bool showFruits = collapsing || treeGrowth >= 0.75f;
    if (showFruits && treeFruitCount > 0) {
        float fruitProgress = 1.0f;
        if (!collapsing) {
            fruitProgress = (treeGrowth - 0.75f) / 0.25f;
            if (fruitProgress > 1.0f) fruitProgress = 1.0f;
        }

        uint8_t visibleFruits = (uint8_t)((float)treeFruitCount * fruitProgress + 0.5f);
        if (visibleFruits > treeFruitCount) visibleFruits = treeFruitCount;

        for (uint8_t i = 0; i < visibleFruits; i++) {
            const TreeFruit& f = treeFruits[i];
            int16_t fx = bx + f.offsetX + sway;
            int16_t fy = baseY + f.offsetY;

            if (collapsing) {
                int16_t distFromGround = baseY - fy;
                if (distFromGround < 0) distFromGround = 0;
                int16_t fruitDrop = (int16_t)(collapseT2 * (float)distFromGround * 1.5f);
                fy += fruitDrop;
                if (fy > baseY) continue;
            }

            if (treePhase == TreePhase::ALIVE) {
                uint32_t phase = now + (uint32_t)f.bobPhase * 8;
                int wave2 = (int)(phase % 2000);
                int bob = (wave2 < 1000) ? 0 : PX;
                fy += bob;
            }

            fatFruit(canvas, fx, fy, f.radius, fruitFill, fruitLine);
        }
    }

    } // end Phase 2-3

    // --- Dropping fruits (gravity fall after deauth) ---
    for (uint8_t i = 0; i < MAX_DROPPING; i++) {
        if (!droppingFruits[i].active) continue;

        float t = (float)(now - droppingFruits[i].dropStart) / 1000.0f;
        int16_t fallDist = (int16_t)(0.5f * 800.0f * t * t);
        int16_t currentY = droppingFruits[i].y + fallDist;

        if (currentY >= 106) {
            droppingFruits[i].active = false;

            uint8_t splashCount = 3 + (uint8_t)(esp_random() % 2);
            for (uint8_t s = 0; s < splashCount; s++) {
                FruitSplash& sp = fruitSplashes[fruitSplashIdx];
                fruitSplashIdx = (fruitSplashIdx + 1) % FRUIT_SPLASH_COUNT;
                sp.x = (float)droppingFruits[i].x;
                sp.y = 104.0f;
                sp.vx = ((float)(esp_random() % 600) - 300.0f) / 100.0f;
                sp.vy = -2.0f - (float)(esp_random() % 300) / 100.0f;
                sp.size = 1 + (uint8_t)(esp_random() % (droppingFruits[i].radius / 2 + 1));
                sp.spawnTime = now;
                sp.active = true;
            }
            continue;
        }

        fatFruit(canvas, droppingFruits[i].x, currentY, droppingFruits[i].radius, fruitFill, fruitLine);
    }

    // --- Fruit splash particles (burst on ground impact) ---
    static uint32_t lastSplashUpdate = 0;
    if (now - lastSplashUpdate > 50) {
        lastSplashUpdate = now;
        for (uint8_t i = 0; i < FRUIT_SPLASH_COUNT; i++) {
            if (!fruitSplashes[i].active) continue;
            fruitSplashes[i].x += fruitSplashes[i].vx;
            fruitSplashes[i].y += fruitSplashes[i].vy;
            fruitSplashes[i].vy += 0.15f;  // slight gravity pull-back
            if (now - fruitSplashes[i].spawnTime > 500) fruitSplashes[i].active = false;
        }
    }
    for (uint8_t i = 0; i < FRUIT_SPLASH_COUNT; i++) {
        if (!fruitSplashes[i].active) continue;
        int spx = snapPx((int16_t)fruitSplashes[i].x);
        int spy = snapPx((int16_t)fruitSplashes[i].y);
        if (spx < 0 || spx >= DISPLAY_W) continue;
        if ((float)(now - fruitSplashes[i].spawnTime) / 500.0f >= 1.0f) continue;
        canvas.fillRect(spx, spy, PX, PX, splashCol);
    }
}

// --- Night sky star system ---
