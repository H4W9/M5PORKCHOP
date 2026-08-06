// Weather effects module - clouds, rain, thunder, wind
// Mood-tied weather system ported from Sirloin

#include "weather.h"
#include "avatar.h"
#include "mood.h"
#include "../ui/display.h"
#include "../core/xp.h"
#include "../audio/sfx.h"
#include <esp_random.h>

namespace Weather {

// === CLOUD PARALLAX STATE ===
static char cloudPattern[40] = {0};
static bool cloudMoving = true;  // Always drift
static bool cloudDirection = true;  // true = right
static uint32_t lastCloudUpdate = 0;
static uint16_t cloudSpeed = 14400;  // Ultra slow atmospheric drift (matches sirloin)
static uint32_t lastCloudParallax = 0;
static const uint8_t CLOUD_PARALLAX_GRASS_SHIFTS = 6;  // Shift clouds every N grass shifts

// === RAIN STATE ===
struct RainDrop {
    float x;
    float y;
    uint8_t speed; // pixels per update for visible rain
};
static const int RAIN_DROP_COUNT = 25;  // Increased for denser rain
static RainDrop rainDrops[RAIN_DROP_COUNT] = {{0}};
static bool rainActive = false;
static bool rainDecided = false;  // Prevents per-frame re-randomization
static int lastMoodTier = -1;     // Track tier (not raw mood) with hysteresis
static uint32_t lastRainUpdate = 0;
static const uint16_t RAIN_SPEED_MS = 30;  // Fast updates for snappy rain

// === THUNDER STATE ===
static bool thunderFlashing = false;
static uint32_t lastThunderStorm = 0;
static uint32_t thunderFlashStart = 0;
static uint8_t thunderFlashesRemaining = 0;
static uint8_t thunderFlashState = 0;  // 0=off, 1=on
static uint32_t thunderMinInterval = 50000;  // 50-90s between storms (adjusts with mood)
static uint32_t thunderMaxInterval = 90000;

// === LIGHTNING BOLT STATE ===
// A jagged golden bolt strikes the grass on whichever side of the screen the
// pig ISN'T standing on, then blooms into a starburst + splash (same shape as
// a bird impact, recolored gold). The bolt lands and finishes blooming BEFORE
// the screen itself flashes, so it reads as the cause of the flash.
static bool boltActive = false;              // bolt currently drawing itself in
static uint32_t boltStartTime = 0;
static const uint16_t BOLT_STRIKE_MS = 180;  // time for the bolt to reach the ground
static uint8_t pendingStormFlashes = 0;      // flash count, held until the bolt lands
static const uint8_t BOLT_POINTS = 9;   // more segments = jaggier, lightning-like
static int16_t boltPathX[BOLT_POINTS];
static int16_t boltPathY[BOLT_POINTS];
static constexpr uint16_t BOLT_GOLD = 0xFEA0;  // golden yellow (RGB565)
static float boltImpactX = 0.0f;

struct BoltSplash { float x, y, vx, vy; uint8_t life; bool active; };
static BoltSplash boltSplashes[6];
static bool    boltExplosionActive = false;
static uint8_t boltExplosionRadius = 0;
static uint8_t boltExplosionMaxRadius = 0;
static uint8_t boltExplosionLife = 0;

// === WIND STATE ===
struct WindParticle {
    float x;
    float y;
    float speed;        // px per update tick
    float spawnX;       // X at birth (for distance/shrink calc)
    float maxTravel;    // distance before vanishing (180-280px)
    uint8_t baseSize;   // initial block count 1-3
    bool active;
    bool dirRight;      // travel direction
};
static WindParticle windParticles[6] = {{0}};
static bool windActive = false;
static uint32_t lastWindGust = 0;
static uint32_t windGustDuration = 0;
static uint32_t windGustInterval = 15000;  // 15-30s between gusts
static uint32_t lastWindUpdate = 0;

// === MOOD-BASED WEATHER CONTROL ===
static int currentMood = 50;  // Cached mood level

// Forward declaration
static void resetCloudPattern();
static void shiftCloudPattern(bool direction, bool allowMutation);

// === INITIALIZATION ===
void init() {
    // Init cloud pattern - scattered dots/dashes with spacing
    resetCloudPattern();
    
    // Init wind particles (inactive)
    for (int i = 0; i < 6; i++) {
        windParticles[i].active = false;
    }
    
    lastCloudUpdate = millis();
    lastCloudParallax = lastCloudUpdate;
    lastWindGust = millis();
    lastThunderStorm = millis();
}

static void resetCloudPattern() {
    // Generate textured cloud pattern with multi-segment clusters
    const char cloudChars[] = {'.', '-', '_'};
    
    // Fill with spaces first
    for (int i = 0; i < 39; i++) {
        cloudPattern[i] = ' ';
    }
    cloudPattern[39] = '\0';
    
    int pos = 0;
    while (pos < 36) {
        // Create a cloud entity (2-4 segments for texture)
        int segments = random(2, 5);
        
        for (int s = 0; s < segments && pos < 39; s++) {
            char segChar = cloudChars[random(0, 3)];
            int segLen = random(1, 6);  // 1 to 5 chars per segment
            
            for (int k = 0; k < segLen && pos < 39; k++) {
                cloudPattern[pos++] = segChar;
            }
        }
        
        // Add gap between clouds
        int gap = random(4, 10);  // 4 to 9 spaces
        pos += gap;
    }
}

static void shiftCloudPattern(bool direction, bool allowMutation) {
    if (direction) {
        // Shift right
        char last = cloudPattern[38];
        for (int i = 38; i > 0; i--) {
            cloudPattern[i] = cloudPattern[i - 1];
        }
        cloudPattern[0] = last;
    } else {
        // Shift left
        char first = cloudPattern[0];
        for (int i = 0; i < 38; i++) {
            cloudPattern[i] = cloudPattern[i + 1];
        }
        cloudPattern[38] = first;
    }

    if (allowMutation && random(0, 50) == 0) {
        int pos = random(0, 39);
        if (cloudPattern[pos] != ' ') {
            const char cloudChars[] = {'.', '-', '_'};
            cloudPattern[pos] = cloudChars[random(0, 3)];
        }
    }
}

// === WEATHER STATE CONTROL ===
// Determine which mood tier we're in (with hysteresis to prevent oscillation)
// Hysteresis: need to cross threshold by 5 points to change tier
static int getMoodTier(int mood, int currentTier) {
    // If no current tier, use raw thresholds
    if (currentTier < 0) {
        if (mood <= -40) return 2;      // SAD: high rain chance
        if (mood <= -20) return 1;      // MEH: low rain chance  
        return 0;                        // HAPPY/NEUTRAL: no rain
    }
    
    // Apply hysteresis based on direction of change
    switch (currentTier) {
        case 0:  // Currently HAPPY - need to drop below -25 to become MEH
            if (mood <= -25) return (mood <= -45) ? 2 : 1;
            return 0;
        case 1:  // Currently MEH - need -45 for SAD, -15 for HAPPY
            if (mood <= -45) return 2;
            if (mood > -15) return 0;
            return 1;
        case 2:  // Currently SAD - need to rise above -35 to become MEH
            if (mood > -35) return (mood > -15) ? 0 : 1;
            return 2;
    }
    return 0;
}

void setMoodLevel(int momentum) {
    currentMood = momentum;
    int newTier = getMoodTier(momentum, lastMoodTier);
    
    // Only re-roll rain when actually changing tiers (not every frame!)
    bool shouldReroll = !rainDecided || (newTier != lastMoodTier);
    
    if (shouldReroll) {
        lastMoodTier = newTier;
        rainDecided = true;
        
        bool shouldRain = false;
        
        if (newTier == 2) {
            // SAD/DEPRESSED: 70% rain chance (increased)
            shouldRain = (random(0, 100) < 70);
            // More frequent storms
            thunderMinInterval = 30000;  // 30-60s
            thunderMaxInterval = 60000;
        } else if (newTier == 1) {
            // MEH: 35% rain chance (increased from 20%)
            shouldRain = (random(0, 100) < 35);
            // Occasional storms
            thunderMinInterval = 60000;  // 60-120s
            thunderMaxInterval = 120000;
        } else {
            // Happy/neutral: no rain, clear the sky
            shouldRain = false;
            thunderMinInterval = 999999;  // Effectively disabled
            thunderMaxInterval = 999999;
        }
        
        setRaining(shouldRain);
    }
}

void setRaining(bool active) {
    if (active && !rainActive) {
        // Spawn raindrops staggered across entire screen height for immediate rain
        for (int i = 0; i < RAIN_DROP_COUNT; i++) {
            rainDrops[i].x = (float)random(0, 240);
            // Distribute drops across visible area (stop above grass at Y=88)
            rainDrops[i].y = (float)random(16, 85);
            // Fast rain (5-8 pixels per update)
            rainDrops[i].speed = random(5, 9);
        }
    } else if (!active && rainActive) {
        // Stop any in-flight thunder to avoid stuck flash on clear skies
        thunderFlashing = false;
        thunderFlashState = 0;
        thunderFlashesRemaining = 0;
        lastThunderStorm = millis();
        // ...and any in-flight bolt strike, so it can't freeze mid-air either
        boltActive = false;
        pendingStormFlashes = 0;
        boltExplosionActive = false;
        for (int s = 0; s < 6; s++) boltSplashes[s].active = false;
    }
    rainActive = active;
} 

void triggerThunderStorm() {
    thunderFlashesRemaining = 3;
    lastThunderStorm = millis();
}

// Forward declarations for static update functions
static void updateClouds(uint32_t now);
static void updateRain(uint32_t now);
static void updateThunder(uint32_t now);
static void updateWind(uint32_t now);

// === BIRD SYSTEM ===
// Birds drift across the sky; an OUTGOING (deauth) wave ring downs them, with
// sparks, a ground explosion, splashes, and an XP reward. (Bird SFX omitted.)
static constexpr int16_t BIRD_PX = 3;
static inline int16_t birdSnap(int16_t v) {
    return (v >= 0) ? (v / BIRD_PX) * BIRD_PX : ((v - 2) / BIRD_PX) * BIRD_PX;
}
struct SkyBird { float x; int8_t y; int8_t vx; uint8_t sinePhase; bool active, falling;
                 float fallVy, fallX, fallY, fallStartY; };
struct BirdSpark { float x, y, vx, vy; uint8_t life; };
struct BirdExplosion { float x, y; uint8_t radius, maxRadius, life; bool active; };
struct ImpactSplash { float x, y, vx, vy; uint8_t life; bool active; };
static SkyBird birds[2];
static BirdSpark sparks[6];
static BirdExplosion explosions[2];
static ImpactSplash impactSplashes[6];
static int8_t whistlingBird = -1;
static uint32_t lastBirdUpdate = 0;
static uint32_t nextBirdSpawn = 0;
static const int16_t GROUND_Y = SCENE_GROUND_Y;   // grass ground line (matches avatar)

static void spawnBird() {
    int slot = -1;
    for (int i = 0; i < 2; i++) if (!birds[i].active) { slot = i; break; }
    if (slot < 0) return;
    SkyBird& b = birds[slot];
    b.y = (int8_t)random(3, 15);
    b.sinePhase = 0; b.active = true; b.falling = false;
    bool goRight = random(0, 2) == 0;
    b.vx = goRight ? (int8_t)random(1, 3) : (int8_t)random(-2, 0);
    if (b.vx == 0) b.vx = 1;
    b.x = goRight ? -20.0f : (float)(DISPLAY_W + 20);
}

static void updateBirds(uint32_t now) {
    if (rainActive) {   // no birds in the rain
        for (int i = 0; i < 2; i++) birds[i].active = false;
        for (int i = 0; i < 6; i++) sparks[i].life = 0;
        for (int i = 0; i < 2; i++) explosions[i].active = false;
        for (int i = 0; i < 6; i++) impactSplashes[i].active = false;
        whistlingBird = -1;
        nextBirdSpawn = now + random(15000, 30001);
        return;
    }
    if (now - lastBirdUpdate < 50) return;   // ~20fps tick
    lastBirdUpdate = now;

    if (now >= nextBirdSpawn) { spawnBird(); nextBirdSpawn = now + random(15000, 30001); }

    for (int i = 0; i < 2; i++) {
        if (!birds[i].active) continue;
        SkyBird& b = birds[i];
        if (!b.falling) {
            b.x += (float)b.vx; b.sinePhase++;
            if (b.x < -25.0f || b.x > (float)(DISPLAY_W + 25)) { b.active = false; continue; }
            int16_t drawY = b.y + ((b.sinePhase & 0x08) ? 1 : 0);
            if (Avatar::checkBirdWaveCollision((int16_t)b.x, drawY)) {
                b.falling = true; b.fallVy = -1.5f; b.fallX = b.x;
                b.fallY = (float)drawY; b.fallStartY = (float)drawY;
                SFX::play(SFX::BIRD_HIT);   // electric zap
                if (whistlingBird < 0) whistlingBird = (int8_t)i;
                int spawned = 0;
                for (int s = 0; s < 6 && spawned < 3; s++) {
                    if (sparks[s].life == 0) {
                        sparks[s].x = b.fallX; sparks[s].y = b.fallY;
                        sparks[s].vx = (float)random(-20, 21) / 10.0f;
                        sparks[s].vy = -1.0f - (float)random(0, 15) / 10.0f;
                        sparks[s].life = random(10, 18); spawned++;
                    }
                }
                uint8_t lvl = XP::getLevel(); if (lvl < 1) lvl = 1;
                XP::addXP((uint16_t)(lvl * random(1, 4)));
                Mood::onBirdKill();
            }
        } else {
            b.fallVy += 0.4f; b.fallY += b.fallVy; b.fallX += (float)b.vx * 0.5f;
            // Bomb whistle: descending pitch tracks the fall (1200Hz -> 200Hz)
            if (whistlingBird == i && b.fallY < (float)GROUND_Y) {
                float range = (float)GROUND_Y - b.fallStartY;
                float prog = (range > 0.0f) ? (b.fallY - b.fallStartY) / range : 1.0f;
                if (prog < 0.0f) prog = 0.0f; if (prog > 1.0f) prog = 1.0f;
                SFX::tone((uint16_t)(1200.0f - prog * 1000.0f), 60);
            }
            if (b.fallY > (float)GROUND_Y) {
                if (whistlingBird == i) whistlingBird = -1;
                SFX::play(SFX::BIRD_IMPACT);   // ground thud
                for (int e = 0; e < 2; e++) if (!explosions[e].active) {
                    explosions[e].x = b.fallX; explosions[e].y = (float)GROUND_Y;
                    explosions[e].radius = 0; explosions[e].maxRadius = (uint8_t)random(9, 13);
                    explosions[e].life = 12; explosions[e].active = true; break;
                }
                int splashed = 0;
                for (int s = 0; s < 6 && splashed < 4; s++) if (!impactSplashes[s].active) {
                    impactSplashes[s].x = b.fallX + (float)random(-6, 7);
                    impactSplashes[s].y = (float)GROUND_Y;
                    impactSplashes[s].vx = (float)random(-30, 31) / 10.0f;
                    impactSplashes[s].vy = -1.0f - (float)random(0, 16) / 10.0f;
                    impactSplashes[s].life = (uint8_t)random(12, 19);
                    impactSplashes[s].active = true; splashed++;
                }
                b.active = false;
            }
        }
    }
    for (int s = 0; s < 6; s++) if (sparks[s].life) {
        sparks[s].x += sparks[s].vx; sparks[s].y += sparks[s].vy; sparks[s].vy += 0.25f; sparks[s].life--;
    }
    for (int e = 0; e < 2; e++) if (explosions[e].active) {
        if (explosions[e].radius < explosions[e].maxRadius) explosions[e].radius++;
        else if (--explosions[e].life == 0) explosions[e].active = false;
    }
    for (int s = 0; s < 6; s++) if (impactSplashes[s].active) {
        impactSplashes[s].x += impactSplashes[s].vx; impactSplashes[s].y += impactSplashes[s].vy;
        impactSplashes[s].vy += 0.3f;
        if (--impactSplashes[s].life == 0) impactSplashes[s].active = false;
    }
}

void drawBirds(M5Canvas& canvas, uint16_t colorFG) {
    uint16_t drawColor = isThunderFlashing() ? getColorBG() : colorFG;
    // Realistic: explosions/sparks/splashes read as fire (orange).
    uint16_t fire = isRealisticTheme() ? 0xFC20 : drawColor;
    const int16_t W = DISPLAY_W, H = MAIN_H;

    for (int i = 0; i < 2; i++) {
        if (!birds[i].active) continue;
        const SkyBird& b = birds[i];
        if (!b.falling) {
            int16_t bx = birdSnap((int16_t)b.x);
            int16_t bodyY = birdSnap(b.y + ((b.sinePhase & 0x08) ? BIRD_PX : 0));
            bool wingsUp = (b.sinePhase & 0x04) != 0;
            int16_t wingY = wingsUp ? (bodyY - BIRD_PX) : (bodyY + BIRD_PX);
            canvas.fillRect(bx, wingY, BIRD_PX, BIRD_PX, drawColor);
            canvas.fillRect(bx + 2 * BIRD_PX, wingY, BIRD_PX, BIRD_PX, drawColor);
            canvas.fillRect(bx + BIRD_PX, bodyY, BIRD_PX, BIRD_PX, drawColor);
        } else {
            int16_t fx = birdSnap((int16_t)b.fallX), fy = birdSnap((int16_t)b.fallY);
            if (fy >= 0 && fy < H) {
                canvas.fillRect(fx, fy, BIRD_PX, BIRD_PX, drawColor);
                canvas.fillRect(fx + BIRD_PX, fy, BIRD_PX, BIRD_PX, drawColor);
            }
        }
    }
    for (int s = 0; s < 6; s++) {
        if (sparks[s].life == 0) continue;
        if (sparks[s].life < 4 && (sparks[s].life % 2 == 0)) continue;
        int16_t sx = birdSnap((int16_t)sparks[s].x), sy = birdSnap((int16_t)sparks[s].y);
        if (sx >= 0 && sx < W && sy >= 0 && sy < H) canvas.fillRect(sx, sy, BIRD_PX, BIRD_PX, fire);
    }
    for (int e = 0; e < 2; e++) {
        if (!explosions[e].active) continue;
        if (explosions[e].life < 4 && (explosions[e].life % 2 == 0)) continue;
        int16_t cx = birdSnap((int16_t)explosions[e].x), cy = birdSnap((int16_t)explosions[e].y);
        int16_t r = (int16_t)explosions[e].radius;
        const int16_t pts[][2] = {
            {0,(int16_t)(-r)},{0,r},{(int16_t)(-r),0},{r,0},
            {(int16_t)(r*7/10),(int16_t)(-r*7/10)},{(int16_t)(-r*7/10),(int16_t)(-r*7/10)},
            {(int16_t)(r*7/10),(int16_t)(r*7/10)},{(int16_t)(-r*7/10),(int16_t)(r*7/10)} };
        for (int p = 0; p < 8; p++) {
            int16_t px = birdSnap(cx + pts[p][0]), py = birdSnap(cy + pts[p][1]);
            if (px >= 0 && px < W && py >= 0 && py < H) canvas.fillRect(px, py, BIRD_PX, BIRD_PX, fire);
        }
    }
    for (int s = 0; s < 6; s++) {
        if (!impactSplashes[s].active) continue;
        if (impactSplashes[s].life < 4 && (impactSplashes[s].life % 2 == 0)) continue;
        int16_t sx = birdSnap((int16_t)impactSplashes[s].x), sy = birdSnap((int16_t)impactSplashes[s].y);
        if (sx >= 0 && sx < W && sy >= 0 && sy < H) canvas.fillRect(sx, sy, BIRD_PX, BIRD_PX, fire);
    }
}

// === ANIMATION UPDATES ===
void update() {
    uint32_t now = millis();
    
    // Update clouds (always)
    updateClouds(now);
    
    // Update rain (if active)
    if (rainActive) {
        updateRain(now);
    }
    
    // Update thunder (if raining)
    if (rainActive) {
        updateThunder(now);
    }
    
    // Update wind gusts (periodic)
    updateWind(now);

    // Update sky birds (drift + deauth-wave takedown physics)
    updateBirds(now);
}

static void updateClouds(uint32_t now) {
    if (cloudMoving && now - lastCloudUpdate >= cloudSpeed) {
        lastCloudUpdate = now;
        shiftCloudPattern(cloudDirection, true);
    }

    // Parallax: when grass is moving, nudge clouds in the same direction (slower).
    if (Avatar::isGrassMoving()) {
        uint32_t parallaxInterval = (uint32_t)Avatar::getGrassSpeed() * CLOUD_PARALLAX_GRASS_SHIFTS;
        if (parallaxInterval < 150) parallaxInterval = 150;

        if (now - lastCloudParallax >= parallaxInterval) {
            lastCloudParallax = now;
            shiftCloudPattern(Avatar::isGrassDirectionRight(), false);
        }
    } else {
        lastCloudParallax = now;
    }
}

static void updateRain(uint32_t now) {
    if (now - lastRainUpdate < RAIN_SPEED_MS) return;
    lastRainUpdate = now;
    
    // Calculate horizontal drift based on grass movement (parallax effect)
    float horizontalDrift = 0.0f;
    if (Avatar::isGrassMoving()) {
        uint16_t grassSpeedMs = Avatar::getGrassSpeed();
        if (grassSpeedMs == 0) grassSpeedMs = 1;
        const float grassShiftPixels = 8.0f;  // GRASS_STRIDE (pixels per grass scroll step)
        float grassPixelsPerMs = grassShiftPixels / (float)grassSpeedMs;
        float grassPixelsPerUpdate = grassPixelsPerMs * (float)RAIN_SPEED_MS;
        horizontalDrift = grassPixelsPerUpdate * 0.4f;  // 40% of grass speed
        if (Avatar::isGrassDirectionRight()) {
            horizontalDrift *= -1.0f;
        }
    }
    
    for (int i = 0; i < RAIN_DROP_COUNT; i++) {
        rainDrops[i].y += (float)rainDrops[i].speed;
        rainDrops[i].x += horizontalDrift;
        
        // Wrap horizontally if drifted off screen
        if (rainDrops[i].x < 0.0f) rainDrops[i].x += (float)DISPLAY_W;
        if (rainDrops[i].x >= (float)DISPLAY_W) rainDrops[i].x -= (float)DISPLAY_W;

        // Respawn just below clouds when reaching the grass ground (~106)
        if (rainDrops[i].y >= (float)(GROUND_Y - 3)) {
            rainDrops[i].y = (float)random(16, 23);  // Just below cloud layer
            rainDrops[i].x = (float)random(0, DISPLAY_W);
            rainDrops[i].speed = random(5, 9);  // Fast rain
        }
    }
}

// Picks a ground-strike X on whichever side of the screen the pig ISN'T
// standing on, with a little randomization within that free space.
static int16_t computeBoltStrikeX() {
    int16_t pigL = (int16_t)Avatar::getCurrentX();
    int16_t pigR = pigL + 108;   // pig sprite is ~108px wide
    if (Avatar::isOnRightSide()) {
        int16_t freeW = pigL;                       // open ground to the left
        return (freeW > 24) ? (int16_t)random(12, freeW - 12) : (int16_t)(pigL / 2);
    } else {
        int16_t freeW = DISPLAY_W - pigR;            // open ground to the right
        return (freeW > 24) ? (int16_t)(pigR + random(12, freeW - 12)) : (int16_t)(pigR + freeW / 2);
    }
}

static void spawnBolt() {
    int16_t strikeX = computeBoltStrikeX();
    boltImpactX = (float)strikeX;
    int16_t topY = 6;
    int16_t spanY = GROUND_Y - topY;
    // Y stays evenly spaced + monotonic (the top-down reveal depends on it);
    // the jaggedness comes from X zigzagging side to side. Swings are widest up
    // top and taper toward the ground strike point, with per-point jitter so no
    // two bolts look alike.
    int8_t dir = (random(0, 2) == 0) ? -1 : 1;   // random starting side
    for (int i = 0; i < BOLT_POINTS; i++) {
        boltPathY[i] = topY + (int16_t)((int32_t)spanY * i / (BOLT_POINTS - 1));
        bool endpoint = (i == 0 || i == BOLT_POINTS - 1);
        if (endpoint) {
            boltPathX[i] = strikeX;                // start + ground point are fixed
        } else {
            dir = -dir;                            // alternate sides each segment
            float taper = 1.0f - (float)i / (float)(BOLT_POINTS - 1);
            int16_t amp = (int16_t)(5.0f + 11.0f * taper);  // ~5..16px, wider up top
            boltPathX[i] = strikeX
                         + (int16_t)(dir * (amp / 2 + (int16_t)random(0, amp / 2 + 1)))
                         + (int16_t)random(-2, 3); // ragged edge
        }
    }
    boltActive = true;
    boltStartTime = millis();
}

static void updateThunder(uint32_t now) {
    // Check if time for new storm
    if (!thunderFlashing && !boltActive && thunderFlashesRemaining == 0 && pendingStormFlashes == 0) {
        if (now - lastThunderStorm > thunderMinInterval) {
            uint32_t interval = random(thunderMinInterval, thunderMaxInterval);
            if (now - lastThunderStorm >= interval) {
                // A storm is starting: the bolt strikes first, flashes follow once it lands.
                pendingStormFlashes = (uint8_t)random(2, 4);  // 2-3 flashes
                lastThunderStorm = now;
                spawnBolt();
            }
        }
    }

    // Animate the bolt strike; once it reaches the grass, bloom the impact
    // and hand off to the flash sequence below.
    if (boltActive && now - boltStartTime >= BOLT_STRIKE_MS) {
        boltActive = false;
        boltExplosionActive = true;
        boltExplosionRadius = 0;
        boltExplosionMaxRadius = (uint8_t)random(10, 14);
        boltExplosionLife = 12;
        int splashed = 0;
        for (int s = 0; s < 6 && splashed < 4; s++) if (!boltSplashes[s].active) {
            boltSplashes[s].x = boltImpactX + (float)random(-6, 7);
            boltSplashes[s].y = (float)GROUND_Y;
            boltSplashes[s].vx = (float)random(-30, 31) / 10.0f;
            boltSplashes[s].vy = -1.0f - (float)random(0, 16) / 10.0f;
            boltSplashes[s].life = (uint8_t)random(12, 19);
            boltSplashes[s].active = true; splashed++;
        }
        thunderFlashesRemaining = pendingStormFlashes;
        pendingStormFlashes = 0;
    }

    // Animate the impact bloom + splash independently of the bolt/flash state.
    if (boltExplosionActive) {
        if (boltExplosionRadius < boltExplosionMaxRadius) boltExplosionRadius++;
        else if (--boltExplosionLife == 0) boltExplosionActive = false;
    }
    for (int s = 0; s < 6; s++) if (boltSplashes[s].active) {
        boltSplashes[s].x += boltSplashes[s].vx; boltSplashes[s].y += boltSplashes[s].vy;
        boltSplashes[s].vy += 0.3f;
        if (--boltSplashes[s].life == 0) boltSplashes[s].active = false;
    }

    // Execute flash sequence
    if (thunderFlashesRemaining > 0 && !thunderFlashing) {
        thunderFlashing = true;
        thunderFlashStart = now;
        thunderFlashState = 1;  // Flash ON
        thunderFlashesRemaining--;
    }
    
    if (thunderFlashing) {
        uint32_t elapsed = now - thunderFlashStart;
        
        // Faster flicker: shorter ON/OFF windows
        if (thunderFlashState == 1 && elapsed > random(30, 60)) {
            // Turn flash OFF
            thunderFlashState = 0;
            thunderFlashStart = now;
        } else if (thunderFlashState == 0 && elapsed > random(20, 40)) {
            // Flash complete
            thunderFlashing = false;
            thunderFlashState = 0;
        }
    }
}

static void updateWind(uint32_t now) {
    // No wind during rain
    if (rainActive) {
        if (windActive) {
            windActive = false;
            for (int i = 0; i < 6; i++) windParticles[i].active = false;
        }
        lastWindGust = now;   // don't fire immediately when rain stops
        return;
    }

    // Check for a new gust — much more likely (and directional) while the pig trots
    if (!windActive && now - lastWindGust > windGustInterval) {
        bool grassOn = Avatar::isGrassMoving();
        int spawnChance = grassOn ? 70 : 20;
        if ((int)random(0, 100) < spawnChance) {
            windActive = true;
            windGustDuration = random(2000, 4000);
            lastWindGust = now;
            bool goRight = grassOn ? Avatar::isGrassDirectionRight() : (random(0, 2) == 0);
            for (int i = 0; i < 6; i++) {
                float spawnX = goRight ? (-5.0f - random(0, 40))
                                       : ((float)DISPLAY_W + 5.0f + random(0, 40));
                windParticles[i].x = spawnX;
                windParticles[i].spawnX = spawnX;
                windParticles[i].y = (float)random(20, GROUND_Y - 10);  // span sky down to near grass
                windParticles[i].speed = 2.0f + (float)random(0, 30) / 10.0f;  // 2.0-5.0
                windParticles[i].maxTravel = (float)random(180, 281);
                windParticles[i].baseSize = random(1, 4);  // 1-3
                windParticles[i].active = true;
                windParticles[i].dirRight = goRight;
            }
        } else {
            windGustInterval = grassOn ? random(3000, 8000) : random(15000, 30000);
            lastWindGust = now;
        }
    }

    if (windActive) {
        if (now - lastWindGust > windGustDuration) {
            windActive = false;
            windGustInterval = Avatar::isGrassMoving() ? random(3000, 8000) : random(15000, 30000);
            for (int i = 0; i < 6; i++) windParticles[i].active = false;
        } else if (now - lastWindUpdate > 50) {  // ~20fps
            lastWindUpdate = now;
            for (int i = 0; i < 6; i++) {
                if (!windParticles[i].active) continue;
                float dir = windParticles[i].dirRight ? 1.0f : -1.0f;
                windParticles[i].x += windParticles[i].speed * dir;
                windParticles[i].y += (random(0, 3) - 1) * 0.5f;   // vertical wobble
                float dist = windParticles[i].x - windParticles[i].spawnX;
                if (dist < 0) dist = -dist;
                if (dist >= windParticles[i].maxTravel) windParticles[i].active = false;
            }
        }
    }
}

// === THUNDER FLASH QUERY ===
bool isThunderFlashing() {
    return thunderFlashing && thunderFlashState == 1;
}

bool isRaining() {
    return rainActive;
}

// === DRAWING ===
void drawClouds(M5Canvas& canvas, uint16_t colorFG) {
    // During thunder flash, use inverted color (matches sirloin's getDrawColor)
    uint16_t drawColor = isThunderFlashing() ? getColorBG() : colorFG;
    
    canvas.setTextSize(2);
    canvas.setTextColor(drawColor);
    canvas.setTextDatum(top_left);
    
    // Draw in sky below top bar, above pig's head
    int cloudY = 2;  // Near top of main canvas
    canvas.drawString(cloudPattern, 0, cloudY);
}

// Blocky Bresenham line, thickened by one extra column, matching the game's
// chunky 3px pixel-art style (same grid as birdSnap/BIRD_PX).
static void drawBoltLine(M5Canvas& canvas, int16_t x1, int16_t y1,
                          int16_t x2, int16_t y2, uint16_t color) {
    int gx1 = x1 / BIRD_PX, gy1 = y1 / BIRD_PX;
    int gx2 = x2 / BIRD_PX, gy2 = y2 / BIRD_PX;
    int dx = abs(gx2 - gx1), dy = abs(gy2 - gy1);
    int sx = (gx1 < gx2) ? 1 : -1, sy = (gy1 < gy2) ? 1 : -1;
    int err = dx - dy;
    while (true) {
        canvas.fillRect(gx1 * BIRD_PX, gy1 * BIRD_PX, BIRD_PX, BIRD_PX, color);
        canvas.fillRect(gx1 * BIRD_PX + BIRD_PX, gy1 * BIRD_PX, BIRD_PX, BIRD_PX, color);
        if (gx1 == gx2 && gy1 == gy2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; gx1 += sx; }
        if (e2 < dx)  { err += dx; gy1 += sy; }
    }
}

// Draws the in-flight bolt (wiped in top-to-bottom) plus the golden
// starburst + splash that blooms once it lands — same shapes as the bird
// impact, just always gold instead of following the flash/inverted colors.
static void drawBoltImpl(M5Canvas& canvas) {
    if (boltActive) {
        uint32_t elapsed = millis() - boltStartTime;
        float t = (float)elapsed / (float)BOLT_STRIKE_MS;
        if (t > 1.0f) t = 1.0f;
        int16_t revealY = boltPathY[0] + (int16_t)((float)(GROUND_Y - boltPathY[0]) * t);
        for (int i = 0; i < BOLT_POINTS - 1; i++) {
            int16_t y1 = boltPathY[i], y2 = boltPathY[i + 1];
            if (y1 >= revealY) break;
            int16_t x1 = boltPathX[i], x2 = boltPathX[i + 1];
            if (y2 <= revealY) {
                drawBoltLine(canvas, x1, y1, x2, y2, BOLT_GOLD);
            } else {
                float segT = (y2 > y1) ? (float)(revealY - y1) / (float)(y2 - y1) : 0.0f;
                int16_t midX = x1 + (int16_t)((float)(x2 - x1) * segT);
                drawBoltLine(canvas, x1, y1, midX, revealY, BOLT_GOLD);
                break;
            }
        }
    }

    if (boltExplosionActive && !(boltExplosionLife < 4 && (boltExplosionLife % 2 == 0))) {
        int16_t cx = birdSnap((int16_t)boltImpactX), cy = birdSnap(GROUND_Y);
        int16_t r = (int16_t)boltExplosionRadius;
        const int16_t pts[][2] = {
            {0,(int16_t)(-r)},{0,r},{(int16_t)(-r),0},{r,0},
            {(int16_t)(r*7/10),(int16_t)(-r*7/10)},{(int16_t)(-r*7/10),(int16_t)(-r*7/10)},
            {(int16_t)(r*7/10),(int16_t)(r*7/10)},{(int16_t)(-r*7/10),(int16_t)(r*7/10)} };
        for (int p = 0; p < 8; p++) {
            int16_t px = birdSnap(cx + pts[p][0]), py = birdSnap(cy + pts[p][1]);
            if (px >= 0 && px < DISPLAY_W && py >= 0 && py < MAIN_H)
                canvas.fillRect(px, py, BIRD_PX, BIRD_PX, BOLT_GOLD);
        }
    }
    for (int s = 0; s < 6; s++) {
        if (!boltSplashes[s].active) continue;
        if (boltSplashes[s].life < 4 && (boltSplashes[s].life % 2 == 0)) continue;
        int16_t sx = birdSnap((int16_t)boltSplashes[s].x), sy = birdSnap((int16_t)boltSplashes[s].y);
        if (sx >= 0 && sx < DISPLAY_W && sy >= 0 && sy < MAIN_H)
            canvas.fillRect(sx, sy, BIRD_PX, BIRD_PX, BOLT_GOLD);
    }
}

void draw(M5Canvas& canvas, uint16_t colorFG, uint16_t colorBG) {
    // During thunder flash, invert colors for rain/wind (matches sirloin)
    uint16_t drawColor = isThunderFlashing() ? colorBG : colorFG;
    // Realistic theme: rain reads as light blue (not thunder-flashing).
    uint16_t rainColor = (isRealisticTheme() && !isThunderFlashing()) ? 0x5D1F : drawColor;

    // Lightning bolt + impact bloom (always gold, drawn before the flash it precedes)
    drawBoltImpl(canvas);

    // Draw rain
    if (rainActive) {
        for (int i = 0; i < RAIN_DROP_COUNT; i++) {
            int x = (int)rainDrops[i].x;
            int y = (int)rainDrops[i].y;
            
            // Skip if above visible area (drops falling into view)
            if (y < 0) continue;
            
            // Draw 6-pixel tall × 2-pixel wide raindrop (slightly taller for visibility)
            for (int dy = 0; dy < 6; dy++) {
                if (y + dy < GROUND_Y - 3) {  // clip just above the grass ground
                    canvas.drawPixel(x, y + dy, rainColor);
                    if (x + 1 < DISPLAY_W) canvas.drawPixel(x + 1, y + dy, rainColor);
                }
            }
        }
    }

    // Draw wind as directional fat-pixel streaks that shrink over their travel.
    if (windActive) {
        for (int i = 0; i < 6; i++) {
            if (!windParticles[i].active) continue;
            int16_t wx = birdSnap((int16_t)windParticles[i].x);
            int16_t wy = birdSnap((int16_t)windParticles[i].y);
            if (wx < -BIRD_PX || wx > DISPLAY_W + BIRD_PX) continue;
            float dist = windParticles[i].x - windParticles[i].spawnX;
            if (dist < 0) dist = -dist;
            float progress = dist / windParticles[i].maxTravel;
            if (progress > 1.0f) progress = 1.0f;
            int blocks = (int)((float)windParticles[i].baseSize * (1.0f - progress) + 0.5f);
            if (blocks < 1) continue;
            for (int b = 0; b < blocks; b++) {
                int16_t bx = windParticles[i].dirRight ? (wx + b * BIRD_PX) : (wx - b * BIRD_PX);
                if (bx >= 0 && bx < DISPLAY_W && wy >= 0 && wy < MAIN_H)
                    canvas.fillRect(bx, wy, BIRD_PX, BIRD_PX, drawColor);
            }
        }
    }
}

}  // namespace Weather
