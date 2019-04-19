#include<string>
#include<chrono>
#include<iostream>
#include<vector>
#include "State.hpp"
#include<GL/glew.h>
#include<glm/glm.hpp>
#include<GLFW/glfw3.h>

std::vector<float> computeRotationMatrix(double angle) {
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

void update(State& state) {
    double F_theta = (state.tl_on ? TORQUE_L : 0) + (state.tr_on ? TORQUE_R : 0) - MASS * GRAVITY_FORCE * std::sin(state.theta);
    double angularMomentumUpdate = F_theta * SECONDS_BETWEEN_FRAMES / MOMENT_OF_INERTIA;
    // L = I*omega
    //if ( (state.L + angularMomentumUpdate) / MOMENT_OF_INERTIA <= MAX_VELOCITY and 
    //     (state.L + angularMomentumUpdate) / MOMENT_OF_INERTIA >= MIN_VELOCITY)
    state.L += angularMomentumUpdate;
        
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
    if (state.player == "human") {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            state.tl_on = true;
        else
            state.tl_on = false;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            state.tr_on = true;
        else
            state.tr_on = false;
    }
    else {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
    }
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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glUseProgram(shader);
    double renderTheta = interpolate(lag / SECONDS_BETWEEN_FRAMES, cur, prev);
    auto rotLocation = glGetUniformLocation(shader, "rot");
    auto rotationMatrix = computeRotationMatrix(renderTheta);
    glUniformMatrix3fv( rotLocation, 1, GL_FALSE, &rotationMatrix[0]);
    glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_BYTE, 0);
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

    /* Human time
    double now = glfwGetTime();
    double last = now;
    double last_action_ts = now;
    */

    auto now = std::chrono::high_resolution_clock::now();
    auto last = now;
    auto last_action_ts = now;


  //  auto t_i = time_of_last_frame;
  //  auto t_im1 = time_of_last_frame;
    double lag = 0;
    double step = 50;
    std::string player;
    std::cout << "Enter human or robot: " << std::endl;
    std::cin >> player;

    Agent Bond;
    State currentState{0.1L, 0.0L, false, false, false, player};
    State prevState{0.1L, 0.0L, false, false, false, player};
    State lastActionState{0.1L, 0.0L, false, false, false, player};

    Action lastAction = Action::off, currentAction = Action::off;
    double reward = Environment::reward(lastActionState, lastAction, currentState);

    do {
        processInput(window, currentState); // TODO: Choose human or robot
        std::cout << "\033[2J";
        std::cout << "Epsilon: " << 1 - EPSILON_C / std::powf(step / 50, 0.5) << std::endl;
        currentState.print();
        std::string act_string;
        switch (currentAction) {
        case Action::off:
            act_string = "Off";
            break;
        case Action::torqueL:
            act_string = "CCW";
            break;
        case Action::torqueR:
            act_string = "CW";
            break;
        }
        std::cout << "Action: " << act_string << std::endl;
        std::cout << "Reward: " << reward << std::endl;
        Bond.print(currentState, currentAction);
        std::cout << "Test" << std::endl;
        // Get cuurent time step 
        last = now;
        //Human time
        //now = glfwGetTime();
        //double dt = now - last;
        /*
        now = std::chrono::high_resolution_clock::now();
        auto dt = std::chrono::duration_cast<std::chrono::microseconds>(now - last);
        lag += dt.count();
        */
        
        // Take action!
        lastActionState = currentState;
        if (currentState.player == "robot")
            act(currentState, currentAction);

        // Compute forces and update according to laws of motion
        std::cout << lag << std::endl;
        std::cout << SECONDS_BETWEEN_FRAMES * 1e6 << std::endl;
        while (lag >= SECONDS_BETWEEN_FRAMES * 1e6) {
            prevState = currentState;
            update(currentState);
            lag -= SECONDS_BETWEEN_FRAMES;
        }

        render(currentState, prevState, lag, shader_programme, window);
        
        auto act_dt = std::chrono::duration_cast<std::chrono::microseconds>(now - last_action_ts);

        if ( act_dt.count() > SECONDS_BETWEEN_ACTIONS) {
            step++;
            last_action_ts = now;
            lastAction = currentAction;
            currentAction = Bond.greedy(currentState, 1 - EPSILON_C / std::powf(step / 50, 0.5));
            reward = Environment::reward(lastActionState, lastAction, currentState);
            Bond.updateSarsa(currentState, currentAction, reward, lastActionState, lastAction);
        }

        if (currentState.broken) {
            currentState = State{2 * M_PI * sample() - M_PI, 0, false, false, false, player};
            currentAction = Bond.greedy(currentState, 1 - EPSILON_C / std::powf(step / 50,0.5));
            
        }
         
        glfwPollEvents();

    } while( glfwWindowShouldClose(window) == 0 );
    
    Bond.dump();

    glfwTerminate();
    return 0;
}
