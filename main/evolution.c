/**********************************************************//**
 * @file evolution.c
 * @brief Driver program for the CREATURE evolution simulator.
 * @version 1.0
 * @author Rena Shinomiya
 * @date March 2017
 **************************************************************/

// Standard library
#include <stdbool.h>        // bool
#include <time.h>           // time
#include <string.h>         // strcpy, strcmp

// External libraries
#ifdef WINDOWS
#include <windows.h>        // OpenGL, GLUT ...
#endif
#include <GL/glew.h>        // OpenGL
#include <GL/glut.h>        // GLUT

// This project
#include "debug.h"          // eprintf, assert
#include "frame_rate.h"     // FrameRate
#include "genetic.h"        // GENETIC
#include "creature.h"       // CREATURE

//**************************************************************
#define FRUSTUM_SIZE 0.1    ///< The scale of the projection frustum.
#define CLIP_NEAR 0.1       ///< Location of the near clipping plane.
#define CLIP_FAR 100.0      ///< Location of the far clipping plane.
#define WINDOW_WIDTH 800    ///< The width of the screen.
#define WINDOW_HEIGHT 600   ///< The height of the screen.
#define FITNESS_TRIALS 10   ///< Number of trials to evaluate fitness.

//**************************************************************
static GENETIC Population;  ///< Genetic algorithm data.
static CREATURE *Creature;  ///< Creature to animate.
static float CameraTheta;   ///< Camera turntable rotation.
static int Seed;            ///< RNG seed for this trial.
static CREATURE Test;       ///< Test creature.
static float CameraX;       ///< Camera X position.
static float CameraY;       ///< Camera Y position.
static bool Rest;           ///< Whether the creature is at rest.

/// Interchangeable fitness function.
static float (*Fitness)(CREATURE *creature);

/**********************************************************//**
 * @brief Draws raster text on the screen.
 * @param line: The line of the screen to render at.
 * @param ...: The string to render on the screen.
 **************************************************************/
#define OutputText(line, ...) {\
    char output[256];\
    sprintf(output, __VA_ARGS__);\
    glWindowPos2i(10, 10 + 13*line);\
    char *message = output;\
    while (*message) {\
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *message);\
        message++;\
    }\
}

/**********************************************************//**
 * @brief Render the current screen.
 **************************************************************/
static void render(void) {
    // Set up this frame
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Camera position
    float totalX = 0.0;
    float totalY = 0.0;
    float totalZ = 0.0;
    for (int i = 0; i < Creature->nNodes; i++) {
        totalX += Creature->nodes[i].position.x;
        totalY += Creature->nodes[i].position.y;
        totalZ += Creature->nodes[i].position.z;
    }
    float averageX = totalX / Creature->nNodes;
    float averageY = totalY / Creature->nNodes;
    float averageZ = totalZ / Creature->nNodes;
    CameraX = (CameraX + averageX) / 2.0;
    if (averageY < 1.5) {
        CameraY = (CameraY + 1.5) / 2;
    } else {
        CameraY = (CameraY + averageY) / 2;
    }
    
    // Camera Rotation
    if (averageZ > 5) {
        CameraTheta = 180;
    } else if (averageZ < -5) {
        CameraTheta = 360;
    } else {
        CameraTheta = 360-(averageZ+5)*18;
    }
    
    // Set the camera position
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(
        CameraX + sin(CameraTheta/180*M_PI)*4, CameraY, cos(CameraTheta/180*M_PI)*4,
        CameraX, 0, 0,
        0, 1, 0
    );
    glMatrixMode(0);
    
    // Draw some information.
    OutputText(0, "%0.1lf FPS", FrameRate());
    OutputText(1, "%0.1f seconds", Creature->clock);
    OutputText(3, "%0.1f meters", CameraX);
    OutputText(2, "%0.1f energy", Creature->energy);
    
    // Draw the floor
    glBegin(GL_LINES);
    for (int i = (averageX - 10); i < (averageX + 40); i++) {
        if (i == 0) {
            glColor3f(0.6, 0.2, 0.2);
            
            // Box bottom
            glVertex3f(i, -0.1, -8.0);
            glVertex3f(i, -0.1, 8.0);
            
            // Box sides
            glVertex3f(i, -0.1, -8.0);
            glVertex3f(i, 4.0, -8.0);
            
            glVertex3f(i, -0.1, 8.0);
            glVertex3f(i, 4.0, 8.0);
            
        } else if (i % 10) {
            glColor3f(0.2, 0.2, 0.2);
            
            // Basic line
            glVertex3f(i, -0.1, -4.0);
            glVertex3f(i, -0.1, 4.0);
            
        } else {
            glColor3f(0.2, 0.6, 0.2);
            
            // Box bottom
            glVertex3f(i, -0.1, -6.0);
            glVertex3f(i, -0.1, 6.0);
            
            // Box sides
            glVertex3f(i, -0.1, -6.0);
            glVertex3f(i, 2.0, -6.0);
            
            glVertex3f(i, -0.1, 6.0);
            glVertex3f(i, 2.0, 6.0);
        }
        
    }
    glEnd();
    
    // Draw the creature
    creature_Draw(Creature);
    
    // Done
    glColor3f(1, 1, 1);
    glFlush();
    glutSwapBuffers();
}

/**********************************************************//**
 * @brief Update the current system.
 **************************************************************/
static void update(void) {
    // Frame update
    RegisterFrame();
    
    // Get the dt
	static float previous = 0.0;
	if (previous <= 0.0) {
		previous = Runtime();
        return;
    }
	float current = Runtime();
    double dt = current - previous;
    previous = current;
    
    // Update the creature's animation
    if (Rest) {
        Rest = creature_Rest(Creature, dt);
    } else {
        creature_Animate(Creature, dt);
    }
    
    // Force redisplay of the screen
    glutPostRedisplay();
}

/**********************************************************//**
 * @brief Timer function for the frame rate library.
 * @return The current system time.
 **************************************************************/
static double timer(void) {
    return glutGet(GLUT_ELAPSED_TIME) / 1000.0;
}

/**********************************************************//**
 * @brief Initialization function.
 * @return Whether the initialization succeeded.
 **************************************************************/
static bool setup(int argc, char **argv) {
    // Random number initialization
    Seed = time(NULL);
    srand(Seed);
    
    // Initialize glut window
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowPosition(80, 80);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutInit(&argc, argv);
    glutCreateWindow("Evolution Simulator");
    
    // Initialize glut callbacks
    glutDisplayFunc(&render);
    glutIdleFunc(&update);
    
    // Initialize glew
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        eprintf("Failed to set up glew.\n");
        return false;
    }
    
    // Clear to white
    glClearColor(0.1, 0.1, 0.1, 1.0);
    glColor3f(1.0, 1.0, 1.0);

    // No backface rendering
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    
    // Depth buffer
    glDepthFunc(GL_LESS);
    glDepthRange(0, 1);
    glEnable(GL_DEPTH_TEST);

    // Projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (float)WINDOW_WIDTH / WINDOW_HEIGHT;
    glFrustum(-FRUSTUM_SIZE*aspect, FRUSTUM_SIZE*aspect, -FRUSTUM_SIZE, FRUSTUM_SIZE, CLIP_NEAR, CLIP_FAR);
    
    // Frame rate library setup
    RegisterTimer(&timer);
    return true;
}

/**********************************************************//**
 * @brief Computes the average NODE position.
 * @param creature: The creature to inspect.
 * @return The average position of the creature's NODEs.
 **************************************************************/
static inline VECTOR AveragePosition(const CREATURE *creature) {
    VECTOR total = {0.0, 0.0, 0.0};
    for (int i = 0; i < creature->nNodes; i++) {
        vector_Add(&total, &creature->nodes[i].position);
    }
    vector_Multiply(&total, 1.0 / creature->nNodes);
    return total;
}

/**********************************************************//**
 * @brief Models the creature walking forward using its
 * MOTION. This is repeated FITNESS_TRIALS times for
 * an averaging effect. The fitness is based on the total
 * distance travelled in the X-direction (positive), and is
 * negatively impacted by significant motion in the Y and Z
 * directions.
 * @param creature: The creature to inspect.
 * @return The fitness of the walk animation.
 **************************************************************/
static float WalkFitness(CREATURE *creature) {
    // Evaluate the creature's walking fitness. To do this we
    // will loop the walking animation ten times
    VECTOR start = AveragePosition(creature);
    VECTOR end;
    
    // Count all positive x motions. However, penalize if there
    // is tons of variance in the Y and Z directions: we only
    // want to go forwards (and repeatably so).
    float xMotionTotal = 0.0;
    float yMotionMagnitudeTotal = 0.0;
    float zMotionMagnitudeTotal = 0.0;
    
    // Do the given number of trials subsequently without
    // resetting the creature.
    for (int trial = 0; trial < FITNESS_TRIALS; trial++) {
        // Perform a whole cycle of the animation
        creature_Animate(creature, BEHAVIOR_TIME);
        
        // Sample the difference again
        end = AveragePosition(creature);
        VECTOR delta = end;
        vector_Subtract(&delta, &start);
        xMotionTotal += delta.x;
        yMotionMagnitudeTotal += fabs(delta.y);
        zMotionMagnitudeTotal += fabs(delta.z);
        start = end;
    }
    
    // Get the final fitness
    float totalFitness = xMotionTotal - yMotionMagnitudeTotal - zMotionMagnitudeTotal;
    return -totalFitness / FITNESS_TRIALS;
}

/**********************************************************//**
 * @brief Computes the fitness.
 * @param entity: The creature to evaluate.
 * @return The fitness, with smaller values being better.
 **************************************************************/
static float EvaluateFitness(void *entity) {
    // Creature cast
    CREATURE *creature = (CREATURE *)entity;
    
    // Reset the creature for evaluation purposes, so the
    // creature always begins at rest and there are no weird
    // initial spasms.
    creature_Reset(creature);
    
    // Check memoized fitness table
    float fitness = creature->fitness;
    if (fitness != FITNESS_INVALID) {
        return fitness;
    }
    
    // We actually need to evaluate the fitness
    // Store the fitness in the memo table
    fitness = Fitness(creature);
    creature->fitness = fitness;
    return fitness;
}

/**********************************************************//**
 * @brief Random creature generation adapter function.
 * @param entity: The CREATURE to generate.
 **************************************************************/
static void create(void *entity) {
    CREATURE *creature = (CREATURE *)entity;
    creature_CreateRandom(creature);
}

/**********************************************************//**
 * @brief Creature breeding adapter function.
 * @param mother: The first parent.
 * @param father: The second parent.
 * @param son: The first childn.
 * @param daughter: The second child.
 **************************************************************/
static void breed(const void *mother, const void *father, void *son, void *daughter) {
    const CREATURE *cMother = (const CREATURE *)mother;
    const CREATURE *cFather = (const CREATURE *)father;
    CREATURE *cSon = (CREATURE *)son;
    CREATURE *cDaughter = (CREATURE *)daughter;
    creature_Breed(cMother, cFather, cSon);
    creature_Breed(cMother, cFather, cDaughter);
}

/**********************************************************//**
 * @brief Game loop and driver function.
 * @param argc: Number of command-line arguments.
 * @param argv: Values for command line arguments.
 * @return Exit code.
 **************************************************************/
int main(int argc, char** argv) {
    // Library setup
    if (!setup(argc, argv)) {
        eprintf("Failed to set up the program.\n");
        return EXIT_FAILURE;
    }
    
    // Initialize variables
    CameraTheta = 270.0;
    CameraX = 0.0;
    CameraY = 1.5;
    Rest = true;
    
    // Command-line variables
    char filename[256];
    enum {
        MODE_EVOLVE,
        MODE_PLAYBACK,
    } mode = MODE_EVOLVE;
    
    // The GENETIC algorithm configuration data.
    GENETIC_REQUEST request = {
        .entitySize = sizeof(CREATURE),
        .populationSize = 1000,
        .random = &create,
        .breed = &breed,
        .fitness = EvaluateFitness,
    };
    
    // Mode reading
    if (argc == 1 || !strcmp(argv[1], "forward")) {
        // Forward walking optimization
        mode = MODE_EVOLVE;
        Fitness = &WalkFitness;
        
    } else if (!strcmp(argv[1], "play")) {
        // Creature playback phase
        if (argc > 2) {
            strcpy(filename, argv[2]);
            mode = MODE_PLAYBACK;
        } else {
            printf("Error: No creature playback file specified.\n");
            exit(-1);
        }
        
    } else {
        // Error
        printf("Error: No mode \"%s\".\n", argv[1]);
        exit(-1);
    }
    
    // Mode
    switch (mode) {
        case MODE_EVOLVE: {
                // Set up the genetic data
                if (!genetic_Create(&Population, &request)) {
                    eprintf("Failed to initialize genetic algorithm.\n");
                    return false;
                }
                
                // Genetic algorithm optimization
                double startTime = Runtime();
                printf("Seed %d\n", Seed);
                int generation = 1;
                while (generation < 100) {
                    genetic_Generation(&Population);
                    Creature = (CREATURE *)genetic_Best(&Population);
                    printf("Generation %d: ", generation);
                    printf("Fitness %0.2f, ", genetic_BestFitness(&Population));
                    printf("Time %0.2lf\n", Runtime() - startTime);
                    generation++;
                }
                
                // Save best creature
                sprintf(filename, "%d_%d.creature", Seed, generation);
                FILE *file = fopen(filename, "wb");
                if (file) {
                    printf("Writing best creature to \"%s\".\n", filename);
                    fwrite(Creature, sizeof(CREATURE), 1, file);
                    fclose(file);
                }
                break;
        }
            
        case MODE_PLAYBACK: {
            // Load the file
            FILE *file = fopen(filename, "rb");
            if (!file) {
                printf("Failed to open \"%s\".\n", argv[1]);
                exit(-1);
            }
            size_t nRead = fread(&Test, sizeof(CREATURE), 1, file);
            (void)nRead;
            fclose(file);
            Creature = &Test;
        }
        
        default:
            break;
    }
    
    // Window title
    char title[256];
    sprintf(title, "%s - Evolution Simulator", filename);
    glutSetWindowTitle(title);
    
    // Reset the creature's animation
    creature_Reset(Creature);
    
    // Main loop and termination
    glutMainLoop();
    return EXIT_SUCCESS;
}

/*============================================================*/
