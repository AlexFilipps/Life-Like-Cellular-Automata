# Life-Like-Cellular-Automata

This project started as my own custom implementation of Conway's "Game of Life" with the goal of simulating relatively large board-states in realtime. It then expanded to allow for the simulation of any Outer-totalistic cellular automaton (also referred to as “life-like” cellular automata). While that is the current limit of the project for now, with minimal additions it could be made to support further rulesets, such as larger neighborhood CAs or multi-neighborhood CAs.

For a brief explanation of the project and some examples of the incredible visuals it can generate please see this video I made to showcase it: [Link](https://www.youtube.com/watch?v=tAYsf5HJKIA&ab_channel=ElegantAlgorithms)
# Included Files

This project includes the main project files as well as a few shader files used for the computation and displaying of the board. The two .computes files are used for calculating cell states at each update (every frame by default). cell_solver.computes is used for normal simulation of the CAs, and cell_solver_age.computes contains an additional constraint I added to limit cell lifespan, allowing for some impressive visuals. The other two shader files are used to display computed textures to the screen.

The rest of the code is mostly in main.cpp which contains the main display loop, and several handlers for drawing to the board and changing rulestrings at runtime.
