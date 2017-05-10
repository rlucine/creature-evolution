/**********************************************************//**
 * @file evolution.c
 * @brief Driver program for the CREATURE evolution simulator.
 * @version 1.0
 * @author Alec Shinomiya
 * @date March 2017
 **************************************************************/

// Standard library
#include <stdbool.h>        // bool
#include <time.h>           // time

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

//**************************************************************
static GENETIC Population;  /// Genetic algorithm data.
static CREATURE *Creature;  /// Creature to animate.
static float CameraTheta;   /// Camera turntable rotation.
static int Seed;            /// RNG seed for this trial.
static CREATURE Test;       /// Test creature.
static float CameraX;       /// Camera X position.
static bool Rest;           /// Whether the creature is at rest.

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
 * @brief Controls the camera's zoom and rotation. This
 * animates the camera using a turntable style.
 * @param key: The key that was pressed. This assumes the arrow
 * keys are used (left/right to rotate, up/down to zoom).
 * @param mouseX: Mouse X position when the key was pressed.
 * This is ignored.
 * @param mouseY: Mouse Y position when the key was pressed.
 * This is ignored.
 **************************************************************/
void keyboard(int key, int mouseX, int mouseY) {
    (void)mouseX;
    (void)mouseY;
    
    // Control the camera's zoom and rotation
    if (key == GLUT_KEY_LEFT) {
        CameraTheta -= 4;
    } else if (key == GLUT_KEY_RIGHT) {
        CameraTheta += 4;
    }
    glutPostRedisplay();
}

/**********************************************************//**
 * @brief Render the current screen.
 **************************************************************/
static void render(void) {
    // Set up this frame
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Camera position
    float totalX = 0.0;
    for (int i = 0; i < Creature->nNodes; i++) {
        totalX += Creature->nodes[i].position.x;
    }
    float averageX = totalX / Creature->nNodes;
    CameraX = (CameraX + averageX) / 2.0;
    
    // Set the camera position
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(
        CameraX + sin(CameraTheta/180*M_PI)*4, 1.5, cos(CameraTheta/180*M_PI)*4,
        CameraX, 0, 0,
        0, 1, 0
    );
    glMatrixMode(0);
    
    // Draw some information.
    OutputText(0, "%0.1lf FPS", FrameRate());
    OutputText(1, "%0.1f seconds", Creature->clock);
    OutputText(2, "%0.1f meters", CameraX);
    
    // Draw the floor
    glBegin(GL_LINES);
        // Floor square
        for (int i = 0; i < 100; i++) {
            if ((i % 10)) {
                glColor3f(0.2, 0.2, 0.2);
                
                // Basic line
                glVertex3f(i, -0.1, -4.0);
                glVertex3f(i, -0.1, 4.0);
                
            } else {
                glColor3f(0.2, 0.4, 0.2);
                
                // Box bottom
                glVertex3f(i, -0.1, -8.0);
                glVertex3f(i, -0.1, 8.0);
                
                // Box sides
                glVertex3f(i, -0.1, -8.0);
                glVertex3f(i, 2.0, -8.0);
                
                glVertex3f(i, -0.1, 8.0);
                glVertex3f(i, 2.0, 8.0);
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
    glutSpecialFunc(&keyboard);
    
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
 * @brief Random creature generation adapter function.
 * @param entity: The CREATURE to generate.
 **************************************************************/
static void random(void *entity) {
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
 * @brief Creature fitness adapter function.
 * @param entity: The CREATURE to test.
 * @return Smaller values for greater fitness.
 **************************************************************/
static float fitness(void *entity) {
    CREATURE *creature = (CREATURE *)entity;
    return -creature_Fitness(creature);
}

/// The GENETIC algorithm configuration data.
static const GENETIC_REQUEST REQUEST = {
    .entitySize = sizeof(CREATURE),
    .populationSize = 1000,
    .random = &random,
    .breed = &breed,
    .fitness = &fitness,
};

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
    Rest = true;
    
    // Mode
    if (argc == 1) {
        // Set up the genetic data
        if (!genetic_Create(&Population, &REQUEST)) {
            eprintf("Failed to initialize genetic algorithm.\n");
            return false;
        }
        
        // Genetic algorithm optimization
        printf("Seed %d\n", Seed);
        int generation = 1;
        while (generation < 250) {
            genetic_Generation(&Population);
            Creature = (CREATURE *)genetic_Best(&Population);
            printf("Generation %d: ", generation);
            printf("Fitness %.2f\n", genetic_BestFitness(&Population));
            generation++;
        }
        
        // Save best creature
        char filename[256];
        sprintf(filename, "%d_%d.creature", Seed, generation);
        FILE *file = fopen(filename, "wb");
        if (file) {
            printf("Writing best creature to \"%s\"\n", filename);
            fwrite(Creature, sizeof(CREATURE), 1, file);
            fclose(file);
        }
        
    } else if (argc == 2) {
        // Load the file
        FILE *file = fopen(argv[1], "rb");
        if (!file) {
            printf("Failed to open \"%s\"\n", argv[1]);
            exit(-1);
        }
        fread(&Test, sizeof(CREATURE), 1, file);
        fclose(file);
        Creature = &Test;
    }
    
    // Reset the creature's animation
    creature_Reset(Creature);
    
    // Main loop and termination
    glutMainLoop();
    return EXIT_SUCCESS;
}

/*============================================================*/
