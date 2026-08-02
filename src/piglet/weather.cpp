// Weather effects module - clouds, rain, thunder, wind
// Mood-tied weather system ported from Sirloin

#include "weather.h"
#include "avatar.h"
#include "mood.h"
#include "../ui/display.h"
#include "../core/xp.h"
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

// === WIND STATE ===
struct WindParticle {
    float x;
    float y;
    float speed;
    bool active;
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
static const int16_t GROUND_Y = 106;   // grass ground line (matches avatar)

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
            if (b.fallY > (float)GROUND_Y) {
                if (whistlingBird == i) whistlingBird = -1;
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
        const float grassShiftPixels = 240.0f / 26.0f;  // screen width / grass pattern chars
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
        if (rainDrops[i].x < 0.0f) rainDrops[i].x += 240.0f;
        if (rainDrops[i].x >= 240.0f) rainDrops[i].x -= 240.0f;
        
        // Respawn just below clouds when reaching bottom
        // Grass starts at Y=91, stop rain 3px above it
        if (rainDrops[i].y >= 88.0f) {
            rainDrops[i].y = (float)random(16, 23);  // Just below cloud layer
            rainDrops[i].x = (float)random(0, 240);
            rainDrops[i].speed = random(5, 9);  // Fast rain
        }
    }
}

static void updateThunder(uint32_t now) {
    // Check if time for new storm
    if (!thunderFlashing && thunderFlashesRemaining == 0) {
        if (now - lastThunderStorm > thunderMinInterval) {
            uint32_t interval = random(thunderMinInterval, thunderMaxInterval);
            if (now - lastThunderStorm >= interval) {
                // Start new storm
                thunderFlashesRemaining = random(2, 4);  // 2-3 flashes
                lastThunderStorm = now;
            }
        }
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
    // Check for new wind gust
    if (!windActive && now - lastWindGust > windGustInterval) {
        // 30% chance of wind gust
        if (random(0, 100) < 30) {
            windActive = true;
            windGustDuration = random(2000, 4000);  // 2-4 second gust
            lastWindGust = now;
            
            // Spawn wind particles
            for (int i = 0; i < 6; i++) {
                windParticles[i].x = -10.0f - random(0, 50);  // Off-screen left
                windParticles[i].y = (float)random(20, 90);
                windParticles[i].speed = (float)random(3, 6);
                windParticles[i].active = true;
            }
        } else {
            // Reset interval for next check
            windGustInterval = random(15000, 30000);
            lastWindGust = now;
        }
    }
    
    // Update active wind particles
    if (windActive) {
        if (now - lastWindGust > windGustDuration) {
            // Gust finished
            windActive = false;
            windGustInterval = random(15000, 30000);
            for (int i = 0; i < 6; i++) {
                windParticles[i].active = false;
            }
        } else {
            // Animate particles
            if (now - lastWindUpdate > 50) {  // ~20fps
                lastWindUpdate = now;
                for (int i = 0; i < 6; i++) {
                    if (windParticles[i].active) {
                        windParticles[i].x += windParticles[i].speed;
                        // Add slight vertical wobble
                        windParticles[i].y += (random(0, 3) - 1) * 0.5f;
                        
                        // Deactivate when off-screen right
                        if (windParticles[i].x > 250.0f) {
                            windParticles[i].active = false;
                        }
                    }
                }
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

void draw(M5Canvas& canvas, uint16_t colorFG, uint16_t colorBG) {
    // During thunder flash, invert colors for rain/wind (matches sirloin)
    uint16_t drawColor = isThunderFlashing() ? colorBG : colorFG;
    // Realistic theme: rain reads as light blue (not thunder-flashing).
    uint16_t rainColor = (isRealisticTheme() && !isThunderFlashing()) ? 0x5D1F : drawColor;

    // Draw rain
    if (rainActive) {
        for (int i = 0; i < RAIN_DROP_COUNT; i++) {
            int x = (int)rainDrops[i].x;
            int y = (int)rainDrops[i].y;
            
            // Skip if above visible area (drops falling into view)
            if (y < 0) continue;
            
            // Draw 6-pixel tall × 2-pixel wide raindrop (slightly taller for visibility)
            for (int dy = 0; dy < 6; dy++) {
                if (y + dy < 88) {  // Clip 3px above grass (grass starts at Y=91)
                    canvas.drawPixel(x, y + dy, rainColor);
                    if (x + 1 < 240) canvas.drawPixel(x + 1, y + dy, rainColor);
                }
            }
        }
    }
    
    // Draw wind particles (ASCII dots)
    if (windActive) {
        canvas.setTextSize(2);
        canvas.setTextColor(drawColor);
        for (int i = 0; i < 6; i++) {
            if (windParticles[i].active) {
                int x = (int)windParticles[i].x;
                int y = (int)windParticles[i].y;
                if (x >= 0 && x < 240) {
                    // Draw as ASCII dot for consistency
                    canvas.drawChar('.', x, y);
                }
            }
        }
    }
}

}  // namespace Weather
