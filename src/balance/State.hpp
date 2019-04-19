#define _USE_MATH_DEFINES
#include <cmath>
#include <stdlib.h>
#include <vector> 
#include <memory>
#include <string>

// Helpers 
double sample(void) {
    return rand() / INT_MAX; 
}

int sample(int start, int end) {
    return (int) (sample() * (end - start)) + start;
}

// Computes angle in range [-pi, pi]
template<typename T>
double angle(T x) {
    x = std::fmod(x + M_PI, 2 * M_PI);
    if (x < 0)
        x += 2 * M_PI;
    return x - M_PI;
    //return x - 2 * M_PI * std::floor( x / (2 * M_PI) );
}

template<typename T>
double angle2pi(T x) {
    x = std::fmod(x, 2 * M_PI);
    if (x < 0)
        x += 2 * M_PI;
    return x;
    //return x - 2 * M_PI * std::floor( x / (2 * M_PI) );
}

constexpr float  EPSILON            =  0.9;
constexpr int    VELOCITY_BUCKETS   =  100;
constexpr int    ANGULAR_BUCKETS    =  100;
constexpr int    NUM_ACTIONS        =  3;
constexpr int    MAX_VELOCITY       =  10;
constexpr int    MIN_VELOCITY       = -10;
constexpr float  LEFT_REWARD_BDY    = -M_PI / 12;
constexpr float  RIGHT_REWARD_BDY   =  M_PI / 12;
constexpr double RADIUS = 0.75;
constexpr double MASS = 1;
constexpr double MOMENT_OF_INERTIA = MASS * RADIUS * RADIUS; // TORQUE = MOMENT_OF_INERTIA * d^2\theta
constexpr double BAR_WIDTH = 0.01;
constexpr double TORQUE_L = 25;
constexpr double TORQUE_R = -25;
constexpr double GRAVITY_FORCE = -9.8196; // m/s^2
constexpr double FRAMERATE = 30.0L;
constexpr double SECONDS_BETWEEN_FRAMES = 1 / FRAMERATE;

enum class Action {
    off,
    torqueL,
    torqueR
};

struct State {
    double theta;
    double L;
    bool tl_on;
    bool tr_on;
    bool broken;
    std::string player;
    void print(void) {
        std::cout << "Angle: " << angle(theta) << std::endl;
        std::cout << "Angular Momentum: " << L << std::endl;
        std::cout << "Torque Left On: " << tl_on << std::endl;
        std::cout << "Torque Right On: " << tr_on << std::endl;
    }
};

class Environment {
public:
    static std::vector<Action> actions;
    static double reward(const State&, Action, State&);
};

std::vector<Action> Environment::actions = {Action::off, Action::torqueL, Action::torqueR};

double Environment::reward(const State& prev, Action a, State& cur) {
    if (cur.L > MAX_VELOCITY or cur.L < MIN_VELOCITY) {
        std::cout << "You broke the rod! Press enter to continue" << std::endl;
        std::cin.ignore();
        cur.broken = true;
        return -100;
    }
    return std::cos(angle(cur.theta)) - 0.9; 

/* OLD REWARD STRUCTURE
    if ( angle(cur.theta) < RIGHT_REWARD_BDY and angle(cur.theta) > LEFT_REWARD_BDY )
        return 1;
    else
        return 0;
*/
}

void act(State& x, Action a) {
    switch (a) {
    case Action::off:
        x.tl_on = false;
        x.tr_on = false;
        break;
    case Action::torqueL:
        x.tl_on = true;
        break;
    case Action::torqueR:
        x.tr_on = true;
        break;
    }
}


class ActionValue {
private:
    std::vector<float> w;

    size_t TileAngleIdx(const State& x, double tileOffset = 0) {
        return std::floor( angle2pi(x.theta) / (2 * M_PI / ANGULAR_BUCKETS) ); 
    }

    size_t TileVeloIdx(const State& x, double tileOffset = 0) {
        return std::floor( (x.L - MIN_VELOCITY) / ( (MAX_VELOCITY - MIN_VELOCITY) / VELOCITY_BUCKETS) );
    }

public:
    ActionValue(void): w(VELOCITY_BUCKETS * ANGULAR_BUCKETS * NUM_ACTIONS, 0) {
        // Initialize weights randomly
        for (int i = 0; i < w.size(); i++)
            w[i] = sample();
    }

    float& operator() (const State& x, Action a) {
        size_t i = TileAngleIdx(x);
        size_t j =  TileVeloIdx(x);
        size_t k;

        switch (a) {
        case Action::off:
            k = 0;
            break;
        case Action::torqueL:
            k = 1;
            break;
        case Action::torqueR:
            k = 2;
            break;
        }

        return w[ i + ANGULAR_BUCKETS * j + k * ANGULAR_BUCKETS * VELOCITY_BUCKETS ];
    }
    
    // Returns pointers to the elements that need to be modified
    std::vector<float *> grad(const State& x, Action a) {

        size_t i = TileAngleIdx(x);
        size_t j =  TileVeloIdx(x);
        size_t k;

        switch (a) {
        case Action::off:
            k = 0;
            break;
        case Action::torqueL:
            k = 1;
            break;
        case Action::torqueR:
            k = 2;
            break;
        }

        return { 
            &w[ i + ANGULAR_BUCKETS * j + k * ANGULAR_BUCKETS * VELOCITY_BUCKETS ] 
        };
    }

    Action greedy(const State& x, double eps = 1) {
        if (sample() < eps) {
            auto a = *std::max_element( 
                Environment::actions.cbegin(),
                Environment::actions.cend(),
                [this, &x] (const Action& a, const Action& b) {
                    return (*this)(x, a) < (*this)(x, b);
                }
            );
            return a; 
        }
        else {
            return Environment::actions[ sample(0, Environment::actions.size()) ]; 
        }
    }
};


class Agent {
private:
    float alpha = 0.5;
    float gamma = 0.99;
    std::unique_ptr<ActionValue> Q;
public:
    Agent(void) {
        Q = std::make_unique<ActionValue>();
    }

    void updateSarsa(const State& cur, Action curAct, double reward, const State& prev, Action prevAct) {
        auto weights_to_update = Q->grad(prev, prevAct);
        for (auto& w : weights_to_update) {
            if (cur.broken) {
                *w = *w + alpha * (reward - (*Q)(prev, prevAct));
            }
            else {
                *w = *w + alpha * (reward + gamma * (*Q)(cur, curAct) - (*Q)(prev, prevAct));
            }
        }
        return;
    }

    Action greedy(const State& x, double epsilon = 1) {
        return Q->greedy(x, epsilon);
    }

    void print(const State& x, Action a) {
        std::cout << "Q(x, a): " << (*Q)(x,a) << std::endl;
    }
};
