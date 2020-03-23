# reinforce_my_learning
A safe place for my attempts to make machines learn how to do human things

This project had two motivations.  First, I wanted to learn some OpenGL graphics programming in C++.  Second,
I wanted to solve a reinforcement learning problem.

I would say that the first goal was a success.  I managed to implement a simple physics engine and visualize my agent
attempting to solve the pole balancing problem.

The second goal, however, leaves a little bit to be desired.  I wound up choosing a discrete tile encoding of my state space for
approximating action-values, and this turns out to be much too coarse.  Agent learning is extremely slow because it needs to 
collect too much information about every possible choice of (angle, angular velocity) it encounters.  Clearly, this is nonsense.
Similar positions and velocities should inform the agent about the best action choice, which they currently do not.  If I were to
continue to work on this, I would try out other state space representations.
