#define _USE_MATH_DEFINES
#include <cmath>
#include <stdlib.h>
#include <vector> 
#include <memory>
#include <string>
#include <fstream>

// Helpers 
double sample(void) {
    return (double) rand() / INT_MAX; 
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

constexpr double TIME_BETWEEN_ACTIONS = 0.5; // 0.5s
constexpr double PHYSICS_TIMESTEP = 0.005; // Update 100x per second 
constexpr int PHYSICS_STEPS_PER_ACTION = 100;

constexpr float  EPSILON_C          =  0.9;
constexpr int    VELOCITY_BUCKETS   =  100;
constexpr int    ANGULAR_BUCKETS    =  100;
constexpr int    NUM_ACTIONS        =  3;
constexpr double MAX_VELOCITY       =  10.0L;
constexpr double MIN_VELOCITY       = -10.0L;
constexpr float  LEFT_REWARD_BDY    = -M_PI / 12;
constexpr float  RIGHT_REWARD_BDY   =  M_PI / 12;
constexpr double RADIUS = 0.75;
constexpr double MASS = 1;
constexpr double MOMENT_OF_INERTIA = MASS * RADIUS * RADIUS; // TORQUE = MOMENT_OF_INERTIA * d^2\theta
constexpr double BAR_WIDTH = 0.01;
constexpr double TORQUE_L = 3;
constexpr double TORQUE_R = -3;
constexpr double GRAVITY_FORCE = -9.8196; // m/s^2
constexpr double FRAMERATE = 30.0L;
constexpr double SECONDS_BETWEEN_FRAMES = 1 / FRAMERATE;
constexpr double ACTION_RATE = 1;
constexpr double SECONDS_BETWEEN_ACTIONS = 1 / ACTION_RATE;

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
        cur.broken = true;
        return -100;
    }
    double act = 0;
    if (a == Action::torqueL or a == Action::torqueR)
        act = 1;

    return -(angle(cur.theta) * angle(cur.theta) + cur.L * cur.L + act);
    //return std::cos(angle(cur.theta)); 

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
        //for (int i = 0; i < w.size(); i++)
        //    w[i] = sample();

        for (auto& wt : w)
            std::cout << wt << std::endl;
    }

    float& weight(int i, int j, int k) {
        return w[ i + ANGULAR_BUCKETS * j + k * ANGULAR_BUCKETS * VELOCITY_BUCKETS ];
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
    float alpha = 1;
    float gamma = 0.75;
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
    
    void dump() {
        std::ofstream ofs ("data.csv", std::ofstream::out); 
        ofs << "action, angle, velocity, Q" << std::endl;
        std::string actn;
        for (int k = 0; k < NUM_ACTIONS; k++) {
            switch (k) {
            case 0:
                actn = "Off";
                break;
            case 1:
                actn = "TorqueL";
                break;
            case 2:
                actn = "TorqueR";
                break;
            }
            for (int i = 0; i < ANGULAR_BUCKETS; i++) 
                for (int j = 0; j < VELOCITY_BUCKETS; j++) 
                    ofs << actn << ", " << i << ", " << j << ", " << Q->weight(i,j,k) << std::endl;
        }
    }

};
