#include<string>
#include<chrono>
#include<iostream>
#include<vector>
#include<GL/glew.h>
#include<glm/glm.hpp>
#include<GLFW/glfw3.h>

std::vector<float> computeRotationMatrix(float angle) {
    std::vector<float> out(9, 0);
    out[0] = std::cos(angle);
    out[1] = std::sin(angle);
    out[2] = 0;
    out[3] = -std::sin(angle);
    out[4] = std::cos(angle);
    out[5] = 0;
    out[6] = 0;
    out[7] = 0;
    out[8] = 1;
    return out;
}

constexpr double RADIUS = 0.75;
constexpr double MASS = 1;
constexpr double MOMENT_OF_INERTIA = MASS * RADIUS * RADIUS; // TORQUE = MOMENT_OF_INERTIA * d^2\theta
constexpr double BAR_WIDTH = 0.005;
constexpr double VELOCITY_X = 1; // screen units per second
constexpr double VELOCITY_Y = 1;
constexpr double TORQUE_L = 1;
constexpr double TORQUE_R = -1;
constexpr double GRAVITY_FORCE = -9.81; // m/s^2

constexpr float FRAMERATE = 120.0f;
constexpr float SECONDS_BETWEEN_FRAMES = 1 / FRAMERATE;

struct State {
    double theta;
    double L;
    bool tl_on;
    bool tr_on;
};

void update(State& state) {
    double F_theta = (state.tl_on ? TORQUE_L : 0) + (state.tr_on ? TORQUE_R : 0) - GRAVITY_FORCE * std::sin(state.theta);
    state.L += F_theta * SECONDS_BETWEEN_FRAMES / MOMENT_OF_INERTIA;
    state.theta += SECONDS_BETWEEN_FRAMES * state.L;
}

double interpolate(double alpha, const State& cur, const State& prev) {
    return alpha * cur.theta + (1 - alpha) * prev.theta;
}

const char* vertex_shader = R"glsl(
    #version 330

    layout(location = 0) in vec3 position;
    uniform mat3 rot;
    void main()
    {
        gl_Position = vec4(rot * position, 1.0);
    }
)glsl";
const char* fragment_shader = R"glsl(
    #version 330
    out vec4 frag_colour;
    void main() {
      frag_colour = vec4(1.0, 1.0, 1.0, 1.0);
    }
)glsl";

void processInput(GLFWwindow* window, State& state) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        state.tl_on = true;
    else
        state.tl_on = false;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        state.tr_on = true;
    else
        state.tr_on = false;
}

// This function opens up a new OpenGL window and does the necessary initialization.  We set a callback to execute if the window is resized
GLFWwindow* initOpenGL(int width, int height) {
    glewExperimental = true; // Needed for core profile
    if( !glfwInit() )
    {
        fprintf( stderr, "Failed to initialize GLFW\n" );
        return nullptr;
    }

    glfwWindowHint(GLFW_SAMPLES, 4); // 4x antialiasing
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // We want OpenGL 3.3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // We don't want the old OpenGL 

    // Open a window and create its OpenGL context
    GLFWwindow* window = glfwCreateWindow( width, height, "Balance a Pole", NULL, NULL);
    if( window == NULL ){
        std::cout << "Failed to create OpenGL window.  Exiting..." << std::endl;
        glfwTerminate();
        return nullptr;
    }

    glfwMakeContextCurrent(window); // Initialize GLEW
    glewExperimental=true; // Needed in core profile
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        return nullptr;
    }

    // Ensure we can capture the escape key being pressed below
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    
    glfwSetFramebufferSizeCallback(
        window, 
        [](GLFWwindow* window, int width, int height) {
            return glViewport(0, 0, width, height);
        }
    );
    return window;
}

void render(State& cur, State& prev, double lag, GLuint shader, GLFWwindow* window) {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear( GL_COLOR_BUFFER_BIT );
    glUseProgram(shader);
    double renderTheta = interpolate(lag / SECONDS_BETWEEN_FRAMES, cur, prev);
    auto rotLocation = glGetUniformLocation(shader, "rot");
    auto rotationMatrix = computeRotationMatrix(renderTheta);
    glUniformMatrix3fv( rotLocation, 1, GL_FALSE, &rotationMatrix[0]);
    //glDrawArrays(GL_TRIANGLES, 0, 3);
    glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_BYTE, 0);
   // time_of_last_frame = std::chrono::system_clock::now();
    glfwSwapBuffers(window);
}

int main(void) {
    // Initialize GLFW
    GLFWwindow* window = initOpenGL(1000, 1000);
    if (window == nullptr) return -1;

    // These are the points we're going to render
    float points[] = {
      -0.5 * BAR_WIDTH,   0.0f, 0.0f,
       0.5 * BAR_WIDTH,   0.0f, 0.0f,
      -0.5 * BAR_WIDTH, RADIUS, 0.0f,
       0.5 * BAR_WIDTH, RADIUS, 0.0f,
        -3 * BAR_WIDTH, RADIUS, 0.0f,
         3 * BAR_WIDTH, RADIUS, 0.0f,
        -3 * BAR_WIDTH, RADIUS + 6 * BAR_WIDTH, 0.0f,
         3 * BAR_WIDTH, RADIUS + 6 * BAR_WIDTH, 0.0f,
    };
    
    // This little guy tells us about which vertices will be used to make triangles
    GLubyte idx[] = {
        0, 1, 2,
        1, 2, 3,
        4, 5, 6,
        5, 6, 7
    };

    // Make a vertex array object to which we'll attach a buffer + props
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    // Here we allocate some memory on the graphics card and move data into the buffer.  We bind the buffer to one of the OpenGL properties GL_ELEMENT_ARRAY_BUFFER or GL_ARRAY_BUFFER
    GLuint vbo = 0;
    glGenBuffers(1, &vbo);  // Make a new object
    glBindBuffer(GL_ARRAY_BUFFER, vbo); // Link the object to our uint vbo
    glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_DYNAMIC_DRAW); // Fill the buffer with some data
    
    // This part tells OpenGL that there is useful data in the buffer.  The second function tells OpenGL how to format the information in the buffer.  Data from the AttribArray streams into the vertex_shader.
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

    GLuint IndexBufferId;
    glGenBuffers(1, &IndexBufferId);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBufferId);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_DYNAMIC_DRAW);

    // Load the shaders
    GLuint vs = glCreateShader(GL_VERTEX_SHADER); // Makes the shader for OpenGL
    glShaderSource(vs, 1, &vertex_shader, NULL);  // Loads the shader source
    glCompileShader(vs);                          // Compile that shit
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragment_shader, NULL);
    glCompileShader(fs);

    GLuint shader_programme = glCreateProgram();
    glAttachShader(shader_programme, fs);
    glAttachShader(shader_programme, vs);
    glLinkProgram(shader_programme);

    
    auto time_of_last_frame = std::chrono::system_clock::now();
    auto t_i = time_of_last_frame;
    auto t_im1 = time_of_last_frame;
    double lag = 0;

    State currentState{0.1L, 0.0L, false, false};
    State prevState{0.1L, 0.0L, false, false};
    
    do {
        // Get cuurent time step 
        auto t_i = std::chrono::system_clock::now();
        std::chrono::duration<double> dt = t_i - t_im1;
        t_im1 = t_i;
        lag += dt.count();
        
        processInput(window, currentState);

        // Compute forces and update according to laws of motion
        while (lag >= SECONDS_BETWEEN_FRAMES) {
            prevState = currentState;
            update(currentState);
            lag -= SECONDS_BETWEEN_FRAMES;
        }
        
        render(currentState, prevState, lag, shader_programme, window);
        glfwPollEvents();

    } // Check if the ESC key was pressed or the window was closed
    while( glfwWindowShouldClose(window) == 0 );
    
    glfwTerminate();
    return 0;
}
