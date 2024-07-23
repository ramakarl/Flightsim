## Flightsim:<br> A simple, single-body flight simulator
## [Rama Karl](http://ramakarl) (c) 2023. MIT License.

The goal of this project is to support simulations with possibly hundreds of objects in flight, therefore the flight model must be robust and simple yet also realistic. This single-body flight model (SBFM) provides a simple and efficient way to simulate flight. The model here is able to simulate power, speed, gravity, wind, banking, stalls and takeoff/landings among other flight capabilities. This is achieved with a single-body model that simplifies many calculations. More realistic flight models typically require the independent simulation of a multiple-surface force model (MSFM) to consider each control surface, yet this increases code complexity and computation. 

<br>
<img src="https://github.com/ramakarl/Flightsim/blob/main/docs/fig09.jpg" width="600" />
Figure 1. Screenshot of this flight simulator showing a landing approach. This simulator introduces a single-body flight model (SBFM) that requires only 7 state variables and 26 lines of code to develop a reasonably realistic model of dynamic flight.

### Basics of Orientation

When one first attempts to code a flight simulator, one of the most interesting aspects is orientation. How is orientation updated? How is it controlled? How does an aircraft move based on orientation?
The essential inputs for orientation control are roll, pitch and yaw. 

<br>
<img src="https://github.com/ramakarl/Flightsim/blob/main/docs/fig01.png" width="600" />
Figure 2. Changes in orientation of an aircraft due to roll, pitch and yaw.<br>

We assume the reader is familiar with basic physics. Acceleration is integrated to update velocity, and velocity is integrated to update position. A basic particle system moves each particle along its own velocity vector. However, in basic particle systems the orientation is often ignored. 

A simple, non-realistic model of orientation is to create a local frame-of-reference (orientation) along the velocity vector. A forward direction (x-axis here) is chosen to align with velocity. The up direction (y-axis here) is initiated as straight up. Roll is a rotation around the x-axis, changing the up direction. Pitch is a rotation around the z-axis, and yaw is a rotation about the y-axis. 

<br>
<img src="https://github.com/ramakarl/Flightsim/blob/main/docs/fig02.png" width="400" />
Figure 3. A basic flight model with a local frame-of-reference centered at point P, with 
forward velocity V along the X-axis.<br><br>

Thus a very basic flight model is this: Ailerons control roll, elevators control pitch, and the rudder controls yaw. Apply these rotations directly to the body orientation. Then, reorient the velocity vector along the forward (x+) direction and move the body forward. The “throttle” of the model can simply control how fast the body is moved forward. 

<br>
<img src="https://github.com/ramakarl/Flightsim/blob/main/docs/fig04.png" width="400" />
Figure 4. A basic non-force model (NFM). Controls directly modify the orientation of the velocity vector.<br><br>

This very simple **non-force model** (NFM) has a nice feeling as a basic flight simulator. The roll/pitch/yaw controls make intuitive sense. The airplane moves forward along the forward direction as expected, and basic maneuvers are possible like banking and loops. This is a good student exercise in orientation.

However, the limits of a non-force model (NFM) are quickly apparent. There is no gravity, no forces, no lift or drag. Therefore the basic challenges of real flight are missing. This model airplane won’t pitch forward or accelerate as it falls. Rolls should cause a plane to dip since the orientation of wing lift is rotated, yet that won’t happen here. Stalls are also not simulated. Without the concept of forces a basic orientation model omits the many challenges of flying.

### Complete Models - Multi-Surface Force Models

A very nice flight simulator model was recently described by Jakob Maier, [Simple Physics-based Flight Simulation with C++](https://www.jakobmaier.at/posts/flight-simulation/). This model provides a “true” model for flight simulation. In physics, a rigid body is an object that has both an orientation and a position which are integrated simultaneously. For position, the integration of acceleration and velocity update body position. For orientation, the integration of total torque and angular velocity update body orientation. This physical model of a rigid body is the correct way to simulate an aircraft.

<br>
<img src="https://github.com/ramakarl/Flightsim/blob/main/docs/fig03.png" width="500" />
Figure 5. A multi-surface flight model (MSFM) where each control surface is modeled as a wing producing torque on the rigid body of the aircraft.<br><br>

The challenge with such a model, where Jakob goes into details, is that the torque produced on an aircraft is quite complex. This depends on the independent torque of each wing control surface on the body. Additionally, the airflow which produces lift and drag forces depend on the current orientation of the aircraft. 

<br>
<img src="https://github.com/ramakarl/Flightsim/blob/main/docs/fig06.png" width="600" />
Figure 6. Program flow in the multi-surface flight model (MSFM). Control surface introduce both force and torque on the rigid body frame. Acceleration is integrated to give velocity and position, while torque and angular velocity are integrated to give orientation.<br><br>

Let’s examine this from a program flow perspective. The control inputs of roll/pitch/yaw modify the multiple wing surfaces, which produce lift and drag that act on both the forces and torques of the aircraft based on its current orientation. While the forces are integrated to update the acceleration, velocity and position, the torques at the current orientation are integrated to update the angular velocity based on inertia to give the updated orientation. The updated orientation then becomes the body orientation in the next step. 

This overall simulation model can be directly and subtly found in Maier’s physics-based flight simulator. I describe this as a **multi-surface force model** (MSFM) because it requires multiple wing surfaces to apply the correct torques so that angular velocity is integrated correctly. This model is able to achieve very good realism and has helped many to explore the coding of flight simulators. If you wish to create a flight simulator with a single user-controlled airplane, as most are, this a realistic approach.

### Motivations for a Single-Body Model

A goal of the current project was to enable hundreds, or more, of aircraft flying simultaneously. The use cases for this includes MMO flight simulators (massive-multiplayer online sims), drone warfare with dozens of aircraft, and multi-agent flight such as social flocking in birds. Simulating hundreds of aircraft requires a core model that is lightweight, simple and computational efficient.
The above multi-surface force model (MSFM) is realistic, yet each aircraft requires the simulation of many wings - typically six at a minimum - to control roll, pitch and yaw. This increases code complexity and computation. I wondered: Is it possible to develop a single-body force model that is reasonably realistic?

My first experiment was simply to take Jakob’s MSFM are remove all the wings. I then applied a lift to the entire aircraft as if it were a single wing. This is a wing-like fuselage with direction and angular velocity. What I found confirms a very basic aspect of aircraft engineering. Most aircraft are designed to be balanced at the pivot point of the wings. If not, they spin out of control (unless there is something fancy like dynamic stability and fly-by-wire). I thought perhaps a well-balanced, wing-like fuselage could provide the basis for a single-body simulator.

<img src="https://github.com/ramakarl/Flightsim/blob/main/docs/fig08.jpg" width="800" />
Figure 7. Wing-like aircraft. While the airframe is stable and produces lift, multiple control surfaces are still required to enable directional stability.<br><br>

Unfortunately, even with a (simulated) model aircraft treated as a giant wing, this didn’t work. The entire wing produces lift as expected, but there is nothing to stabilize it. With all forces in balance it remains stable but if any forces become asymmetric, such as power, wind, angle-of-attack, there is no way to maintain steady flight. There are in fact wing-like aircraft, such as the Northrop YB-35, which look like a giant wing. One might be tempted to say this is a single-body aerodynamic surface. However, there are multiple ailerons on this aircraft which enable one to control it. Therefore, from a physics perspective, practical wing-like aircraft are still multi-body surfaces. The only true single-body aircraft I could imagine is a paper airplane. Notice how these may act like gliders but are essentially impossible to control.

### A Single-Body Force Model

An aircraft which is properly trimmed, with all control surfaces and forces balanced, will maintain steady flight. More importantly, a well-designed aircraft will resist any deviations from steady flight. In aviation this is called static stability. One form of this, directional stability, refers to the ability of an aircraft to resist yawing due to the vertical stabilizer. Other aspects of aircraft design introduce stability on all axes. The primary wings also resist deflection in pitch, encouraging that aircraft to fly along its forward (velocity) vector. This is called longitudinal stability. An aircraft is also lateral stable in roll because there are no dynamic forces that would induce roll (see H.Smith, Longitudinal and Lateral Stability). Static stability may thus be understood as a general principle whereby an aircraft is designed to reorient itself along the forward velocity vector. The intuition that led to the current model is that directional stability might be used to update orientation directly.

Only during 3D flying, or extreme acrobatics, do we find aircraft moving along vectors that do not match their orientation. During stable flight, the torques are such that an aircraft will reorient itself along its direction of motion.

The concept of directional stability provides the basis for a **single-body force model** (SBFM) of flight, as shown in the code here. This model eliminates the integration of torque and angular velocity, and assumes that body orientation should continually attempt to match the forward velocity. With this method there is no need to simulate torque from multiple wings. Like the non-force model (NFM), the pitch/yaw control inputs are directly applied to reorient the velocity vector. Yet the new SBFM model does still integrate acceleration and velocity so many aspects of real flight are retained. 

<br>
<img src="https://github.com/ramakarl/Flightsim/blob/main/docs/fig05.png" width="600" />
Figure 8. A single-body force model (SBFM) which utilizes the principle of directional stability to determine orientation, thereby eliminating the need to simulate multiple control surfaces, while still integrating forces.<br><br>

The SBFM model is as follows: 
The aircraft has a position, velocity and orientation. The current orientation is compared to the velocity, which determines the angle-of-attack of the wings. This directly affects the lift and drag of the primary wings. There are no other lift surfaces (such as elevators or rudder). The lift, drag, throttle and gravity forces are applied to determine acceleration. Acceleration is integrated to update velocity. At this point, the pitch/yaw controls are applied to directly alter the velocity vector. Based on the principle of dynamic stability, the orientation of the aircraft is fractionally re-oriented toward the velocity vector. There is no need to explicitly compute torque, angular velocity or inertial tensors. Velocity is finally integrated to update position.

The entire model can be computed with just a few lines of code, and we don’t need an inner loop over multiple wing surfaces. Although the multi-surface force model (MSFM) is certainly more realistic, many aspects of flight may be simulated by this model while reducing code complexity and computation. The simulation of effects such as pitching forward, falling (unattended yoke), power control, dipping during bank (rotation of lift), and stalls are still represented by this model. 

Parameters to the model control how it behaves. The fractional stability variable (default 0.001) controls how quickly the aircraft is reoriented toward the velocity vector, and thus its overall stability, where 0 is unstable and 1 is totally stable. The input reaction rate (default 0.001) controls how quickly the user roll/pitch/yaw inputs alter the aircraft orientation. Other parameters such as lift and drag factors have their typical meaning in those force equations.

The single-body force model (SBFM) is a major improvement over the basic non-force model (NFM) since the former does compute the body forces of lift, drag, throttle and gravity where the NFM model does not. And while it is somewhat less realistic than the multi-surface force model (MSFM) it is easier to implement and requires less computation. When it comes to basic flight simulation, I found it was difficult to distinguish between the SBFM and MSFM models in terms of behavior. Where the MSFM would really distinguish itself is with 3D flying and acrobatics. When considering most maneuvers in stable flight the SBFM model is generally suitable.
 
The demo here also includes landing and takeoffs. Landings are simply a matter of checking if the roll and pitch are less than five degrees, the sink rate (downward velocity) is less than 2 meters/sec, the current speed is less than 80 meters/sec, and the aircraft is over the runway. Basically, it is possible to land this model.

## Results

The goal of this project is to support the simulation of hundreds of aircraft with a simple, realistic model while being efficient with respect to computation and memory. 

<br>
<img src="https://github.com/ramakarl/Flightsim/blob/main/docs/fig10.png" width="600" />
Figure 10. Results on computation, memory and simulated effects for each model.
<br><br>

## Acknowledgements

I'd like to thank Mark Zifchock for comments on non-force models and Scott Smolka for discussions on flight control in general.

## Building

Steps to build:
1. Build [libmin](https://github.com/ramakarl/libmin) first with cmake.
2. Build Flightsim with cmake. Specify the installed location of libmin as the LIBMIN_PATH.
3. Run!

## Input Controls

LEFT/RIGHT - Ailerons (roll)<br>
UP/DWON - Elevators (pitch)<br>
W/S - Throttle<br>
F - Flaps<br>
C - Change camera<br>

## License

Rama Karl Hoetzlein (c) 2023<br>
MIT License<br>
<br>
Citations:<br>
2023, Hoetzlein, Rama. "A single-body force model for aerodynamic flight". Retrieve from github.com/ramakarl/Flightsim. 



