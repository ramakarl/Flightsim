## Flightsim:<br> A simple,single-body flight model
## [Rama Karl](http://ramakarl) (c) 2023. MIT License.

This single-body flight model (SBFM) provides a simple and efficient way to simulate flight. The model here is able to simulate power, speed, gravity, wind, banking, stalls and takeoff/landings among other flight capabilities. This is achieved with a single-body model that simplifies many calculations. More realistic flight models typically require the independent simulation of a multiple-surface force model (MSFM) to consider each control surface, yet this increases code complexity and computation. The goal of this project was to simulate hundreds of objects in flight, therefore the flight model must be robust and simple yet also realistic. 

<br>
<img src="https://github.com/ramakarl/Flightsim/blob/main/docs/fig09.jpg" width="600" />
Figure 1. Screenshots of this flightsim showing a) takeoff, b) banking and c) landing approach. This simulator introduces a single-body flight model (SBFM) that requires only 7 state variables and 26 lines of code to develop a reasonably realistic model of dynamic flight.

### Basics of Orientation

When one first attempts to code a flight simulator, one of the most interesting aspects is orientation. How is orientation updated? How is it controlled? How does an aircraft move based on orientation?
The essential inputs for orientation control are roll, pitch and yaw. 

<br>
<img src="https://github.com/ramakarl/Flightsim/blob/main/docs/fig01.png" width="600" />
Figure 1. Changes in orientation of an aircraft due to roll, pitch and yaw.

We assume the reader is familiar with basic physics. Acceleration is integrated to update velocity, and velocity is integrated to update position. A basic particle system moves each particle along its own velocity vector. However, in basic particle systems the orientation is often ignored. 

A simple, non-realistic model of orientation is to create a local frame-of-reference (orientation) along the velocity vector. A forward direction (x-axis here) is chosen to align with velocity. The up direction (y-axis here) is initiated as straight up. Roll is a rotation around the x-axis, changing the up direction. Pitch is a rotation around the z-axis, and yaw is a rotation about the y-axis. 

