// Piglet ASCII avatar
#pragma once

#include <M5Unified.h>

enum class AvatarState {
    NEUTRAL,
    HAPPY,
    EXCITED,
    HUNTING,
    SLEEPY,
    SAD,
    ANGRY
};

enum class TreePhase : uint8_t {
    HIDDEN = 0,   // Not visible
    GROWING,      // Trunk -> canopy -> fruits animation
    ALIVE,        // Fully grown, gentle sway + fruit bob
    COLLAPSING    // Reverse animation
};

class Avatar {
public:
    static void init();
    static void draw(M5Canvas& canvas);
    static void setState(AvatarState state);
    static AvatarState getState() { return currentState; }
    static bool isFacingRight();  // Get current facing direction
    static bool isOnRightSide();  // Get screen position (for bubble placement)
    static bool isTransitioning();  // True during walk transition (hide bubble)
    static int getCurrentX();  // Get current animated X position
    
    // Phase 8: Intensity-based animation modifiers
    static void setMoodIntensity(int intensity);  // -100 to 100, affects blink/flip rates
    
    static void blink();
    static void sniff();  // Trigger nose sniff animation (600ms animated cycle)
    static void wiggleEars();
    static void cuteJump();  // Trigger cute celebratory jump (higher than walk bounce)

    // Upstream pig-animation set
    static void perkUp();       // Ears pop + quick upward bounce (new network)
    static void flinch();       // Duck down + jitter (error/heap critical)
    static void spin();         // Rapid direction flips + hop (level-up celebration)
    static void pawScratch();   // Small X oscillation (bored)
    static void triggerTailWiggle();       // Burst tail wag on celebrations
    static void triggerSparkles(uint8_t count = 6);
    static void attackHop();    // Multi-hop pounce (capture/deauth)
    static bool isAttackHopping();

    // Direction control
    static void setFacingLeft();
    static void setFacingRight();
    
    // Attack shake (visual feedback for captures)
    static void setAttackShake(bool active, bool strong);
    
    // Thunder flash (invert colors for weather effect)
    static void setThunderFlash(bool active);
    static bool isThunderFlashing();

    // ---- Fruit tree (hunting "walk up & shake" animation) ----
    static void showTree(uint8_t fruitCount);  // Grow a tree with N fruits
    static void hideTree();                     // Collapse it
    static bool isTreeVisible();
    static void dropFruit();                    // Detach one fruit (deauth success)
    static void drawTree(M5Canvas& canvas);     // Draw behind the pig
    static void generateTree(uint8_t fruitCount);
    static void updateTree();

    struct TreeFruit  { int8_t offsetX, offsetY; uint8_t radius; uint8_t bobPhase; };
    struct TreeBranch { int8_t x1, y1, x2, y2; uint8_t thickness; };
    struct TreeLeafCluster { int8_t cx, cy; uint8_t radius; };
    struct TreeTrunk  { int16_t baseX; uint8_t trunkHeight; uint8_t trunkWidth; int8_t trunkLean; uint8_t crownRadius; };

    static constexpr uint8_t  MAX_BRANCHES = 18;
    static constexpr uint8_t  MAX_LEAF_CLUSTERS = 16;
    static constexpr uint8_t  MAX_TREE_FRUITS = 8;
    static constexpr uint16_t TREE_GROW_MS = 600;
    static constexpr uint16_t TREE_COLLAPSE_MS = 350;
    static constexpr uint16_t TREE_MIN_ALIVE_MS = 4000;

    static TreePhase treePhase;
    static float treeGrowth;
    static uint32_t treeAnimStart;
    static TreeTrunk treeTrunk;
    static TreeBranch treeBranches[MAX_BRANCHES];
    static uint8_t treeBranchCount;
    static TreeLeafCluster treeLeaves[MAX_LEAF_CLUSTERS];
    static uint8_t treeLeafCount;
    static uint8_t treeEndpointLeafCount;
    static TreeFruit treeFruits[MAX_TREE_FRUITS];
    static uint8_t treeFruitCount;
    static uint32_t treeSeed;
    static bool treePendingHide;
    static bool treePendingShow;
    static uint8_t treePendingFruits;
    static uint32_t treeAliveStart;
    static int16_t treeScrollOffset;

    // Night sky star system (RTC-based)
    static bool isNightTime();           // check rtc for night hours, 20:00-06:00
    static bool areStarsActive();        // stars currently visible?
    
    // Walk wind-up animation (smooth slide for coast-back)
    static void startWindupSlide(int targetX, bool faceRight = false);
    
    // Grass animation control (direction: true=right, false=left)
    static void setGrassMoving(bool moving, bool directionRight = true);
    static bool isGrassMoving() { return grassMoving; }
    static bool isGrassDirectionRight() { return grassDirection; }
    static uint16_t getGrassSpeed() { return grassSpeed; }
    static void setGrassSpeed(uint16_t ms);  // Speed in ms per shift (lower = faster)
    static void setGrassPattern(const char* pattern);  // Custom pattern (max 26 chars)
    static void resetGrassPattern();  // Reset to random binary pattern
    
private:
    // Star system state
    struct Star {
        int16_t x;              // screen x range 0-239
        int16_t y;              // screen y range 20-100
        uint8_t size;           // 1-2 px radius
        uint8_t brightness;     // 0-255, 0 means hidden
        bool isBlinking;        // twinkle behavior
        uint32_t fadeInStart;   // when this star started appearing
    };
    static Star stars[15];
    static uint8_t starCount;
    static constexpr uint8_t MAX_STARS = 15;
    static uint32_t lastStarSpawn;
    static uint32_t nextSpawnDelay;
    static bool starsActive;
    static uint32_t lastNightCheck;
    static bool cachedNightMode;

    static void initStarPositions();
    static void updateStars();
    static void drawStars(M5Canvas& canvas);
    static void fillPigBoundingBox(M5Canvas& canvas);
    static AvatarState currentState;
    static bool isBlinking;
    static bool isSniffing;
    static bool earsUp;
    static uint32_t lastBlinkTime;
    static uint32_t blinkInterval;
    static int moodIntensity;  // Phase 8: -100 to 100
    
    // Cute jump animation state
    static bool jumpActive;
    static uint32_t jumpStartTime;
    static constexpr uint16_t JUMP_DURATION_MS = 400;  // Total jump time (up + down)
    static constexpr int JUMP_HEIGHT = 8;  // Pixels to jump up

    // Attack hop (multi-hop pounce on capture/deauth)
    static bool attackHopActive;
    static uint32_t attackHopStartTime;
    static uint8_t attackHopIndex;
    static uint8_t attackHopTotal;
    static int16_t attackHopOriginX;
    static int16_t attackHopTargets[5];
    static constexpr uint16_t ATTACK_HOP_MS = 250;
    static constexpr int16_t ATTACK_HOP_HEIGHT = 10;
    // Spin (level-up)
    static bool spinActive;
    static uint32_t spinStart;
    static constexpr uint16_t SPIN_DURATION_MS = 600;
    static constexpr uint8_t SPIN_FLIPS = 4;
    // Perk up (new network)
    static bool perkUpActive;
    static uint32_t perkUpStart;
    static constexpr uint16_t PERK_UP_DURATION_MS = 200;
    static constexpr int PERK_UP_HEIGHT = 3;
    // Flinch (error)
    static bool flinchActive;
    static uint32_t flinchStart;
    static constexpr uint16_t FLINCH_DURATION_MS = 300;
    // Paw scratch (bored)
    static bool pawScratchActive;
    static uint32_t pawScratchStart;
    static constexpr uint16_t PAW_SCRATCH_DURATION_MS = 800;
    // Tail wiggle burst
    static bool tailWiggleActive;
    static uint32_t tailWiggleStart;
    static constexpr uint16_t TAIL_WIGGLE_DURATION_MS = 800;
    // Sparkle particles
    struct SparkleParticle { int16_t x, y; int8_t vx, vy; uint8_t life; };
    static constexpr uint8_t MAX_SPARKLES = 8;
    static SparkleParticle sparkles[MAX_SPARKLES];
    static void updateAndDrawSparkles(M5Canvas& canvas);
    
    // Walk transition animation
    static bool transitioning;
    static uint32_t transitionStartTime;
    static int transitionFromX;
    static int transitionToX;
    static bool transitionToFacingRight;
    static int currentX;  // Animated X position
    static constexpr uint16_t TRANSITION_DURATION_MS = 400;  // Walk across time
    
    // Grass animation state
    static bool grassMoving;
    static bool grassDirection;  // true = grass scrolls right, false = scrolls left
    static bool pendingGrassStart;  // Wait for transition before starting grass
    static bool onRightSide;  // Track which side of screen pig is on
    static uint32_t lastGrassUpdate;
    static uint16_t grassSpeed;  // ms per shift

    // Fat-pixel grass blade field (pixel-scrolled), matching the original.
    struct GrassBlade {
        uint8_t height;  // 6-20 px
        int8_t  lean;    // tip offset: -3..+3
        uint8_t width;   // base half-width: 1-3
    };
    static constexpr int16_t GRASS_STRIDE   = 8;   // px spacing between blades
    static constexpr uint8_t GRASS_BLADE_MAX = 48; // >= DISPLAY_W/STRIDE for 320-wide
    static GrassBlade grassBlades[GRASS_BLADE_MAX];
    static int16_t grassOffset;  // smooth scroll pixel offset

    static void drawFrame(M5Canvas& canvas, const char** frame, uint8_t lines, bool blink = false, bool faceRight = true, bool sniff = false);
    static void drawGrass(M5Canvas& canvas);
    static void updateGrass();
};
