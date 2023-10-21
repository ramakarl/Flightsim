//--------------------------------------------------------
//
// Flight simulator, by Rama Hoetzlein 2023
// 
// This implements a basic force-based flight simulator with
// lift, drag, thrust and gravity forces. The goal was to create a flight
// model that is compact, fast and simple yet has realistic features.
//
// Features of this model are: 
//    Gliding - forces correctly balance under low or no power
//    Altitude loss - high roll or zero power result in drag causing altitude loss
//    Roll/pitch control - implemented by deflecting the body orientation & fwd velocity
//    Stalls - with zero power, aoa and drag increases, causing stalls.
//    Landing/Take off - ground conditions. zero roll/pitch, ground friction
//    Taxiing - on the ground left/right becomes rudder
//    Wind - modify the m_wind variable to introduce wind
//    Flaps - press the 'f' key for flags. increases drag, useful when landing.
//
// Orientation is a unique challenge. A common way to implement this is to
// treat each wing/control surface as an independent aerofoil acting on a
// single rigid body. While more realistic this complicates code and requires
// significantly more computation (e.g. inertia tensors). Instead we take advantage 
// of the directional stability of aircraft, which tend to reorient toward the forward velocity.
//   See: https://en.wikipedia.org/wiki/Directional_stability
// The body orientation is gradually interpolated toward the fwd velocity with quaternions.
//
// This model still retains a difference between the forward velocity (m_vel)
// and the body forward orientation (fwd), which is needed to compute angle-of-attack 
// for lift forces. However there is no torque, angular velocity or rotational inertia.
// These assumptions can still cause stalls, but not flat spins or 3D flying.
//
//--------------------------------------------------------------------------------
// Copyright 2019-2023 (c) Quanta Sciences, Rama Hoetzlein, ramakarl.com
//
// * Derivative works may append the above copyright notice but should not remove or modify earlier notices.
//
// MIT License:
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and 
// associated documentation files (the "Software"), to deal in the Software without restriction, including without 
// limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, 
// and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES 
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS 
// BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF 
// OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#include <time.h>
#include "main.h"			// window system 
#include "quaternion.h"

#include "gxlib.h"			// low-level render
#include "g2lib.h"			// gui system
using namespace glib;

class Sample : public Application {
public:
	virtual bool init();
	virtual void startup ();
	virtual void display();
	virtual void reshape(int w, int h);
	virtual void motion (AppEnum button, int x, int y, int dx, int dy);	
	virtual void keyboard(int keycode, AppEnum action, int mods, int x, int y);
	virtual void mouse (AppEnum button, AppEnum state, int mods, int x, int y);	
	virtual void mousewheel(int delta);
	virtual void shutdown();

	void		Advance ();
	void		CheckLanding ();
	void		CameraToCockpit();
	void		drawGrid( Vec4F clr );
	
	// state variables
	Vec3F		m_pos, m_vel;
	Quaternion	m_orient;	
	float		m_roll, m_pitch, m_pitch_adv, m_power;

	// extra variables (for rendering/debug)
	Vec3F		m_lift, m_thrust, m_drag, m_force, m_accel;
	float		m_speed, m_max_speed, m_aoa;
	float		m_DT, m_flaps;
	Vec3F		m_wind;

	float		m_runway_length;
	float		m_runway_width;
	std::string m_landing_info;
	bool		m_landing_status;
	int			m_airborn;

	float		m_time;
	bool		m_run, m_flightcam;
	Camera3D*	m_cam;
	int			mouse_down;
};
Sample obj;


bool Sample::init ()
{
	int w = getWidth(), h = getHeight();			// window width &f height
	m_run = true;
	m_flightcam = true;

	addSearchPath ( ASSET_PATH );
	init2D ( "arial" );
	setview2D ( w, h );	
	setTextSz ( 16, 1 );		
	
	m_cam = new Camera3D;
	m_cam->setFov ( 120 );
	m_cam->setNearFar ( 1.0, 100000 );
	m_cam->SetOrbit ( Vec3F(-30,30,0), Vec3F(5,0,0), 10, 1 );

	m_pos.Set (0, 10, 0);
	m_vel.Set (0, 0, 200);
	m_airborn = 1;
	m_roll = 0;
	m_pitch = 0;
	m_power = 3;				// "throttle up"
	m_pitch_adv = 0;
	m_accel.Set(0,0,0);
	m_orient.fromDirectionAndRoll ( Vec3F(0,0,1), m_roll );
	m_flaps = 0;
	m_max_speed = 500.0;		// top speed, 500 m/s = 1800 kph = 1118 mph

	m_DT = 0.001;

	m_runway_length = 2000;		// 2000 meters (6560 ft)
	m_runway_width = 50;		// 50 meters (164 ft)

	m_wind.Set (0, 0, 0);

	return true;
}

void Sample::drawGrid( Vec4F clr )
{
	Vec3F a;
	float o = 0.02;

	// runway
	float x = m_runway_width;
	float z = m_runway_length;
	drawLine3D ( Vec3F(-x, o,-z), Vec3F(-x, o, z), Vec4F(0,0,1,1) );
	drawLine3D ( Vec3F(-x, o, z), Vec3F( x, o, z), Vec4F(0,0,1,1) );
	drawLine3D ( Vec3F( x, o, z), Vec3F( x, o,-z), Vec4F(0,0,1,1) );
	drawLine3D ( Vec3F( x, o,-z), Vec3F(-x, o,-z), Vec4F(0,0,1,1) );	
	for (int n=-z; n < z; n+= 60) {
		drawLine3D ( Vec3F( 1, o, n), Vec3F( 1, o, n+20), Vec4F(1,1,1,1) );
		drawLine3D ( Vec3F(-1, o, n), Vec3F(-1, o, n+20), Vec4F(1,1,1,1) );
	}

	// center section
	o = -0.02;			// offset
	for (int n=-5000; n <= 5000; n += 50 ) {
		drawLine3D ( Vec3F(n, o,-5000), Vec3F(n, o, 5000), Vec4F(.6,.6,.6, .5) );
		drawLine3D ( Vec3F(-5000, o, n), Vec3F(5000, o, n), Vec4F(.6,.6,.6, .5) );
	}
	
	// large sections
	for (int j=-5; j <=5 ; j++) {
		for (int k=-5; k <=5; k++) {
			a = Vec3F(j, 0, k) * Vec3F(5000,0,5000);
			if (j==0 && k==0) continue;
			for (int n=0; n <= 5000; n+= 200) {
				drawLine3D ( Vec3F(a.x,   o, a.z+n), Vec3F(a.x+5000, o, a.z+n), Vec4F(.3,.3,.3, .5) );
				drawLine3D ( Vec3F(a.x+n,-o, a.z  ), Vec3F(a.x+n,    o, a.z+5000), Vec4F(.3,.3,.3, .5) );
			}
		}
	}

}

void Sample::CheckLanding ()
{
	if ( m_airborn > 2000 ) {

		char msg[4096];
		Vec3F angs;
		m_orient.toEuler ( angs );			
		
		// Check for good landing:
		// - Speed < 80 m/s
		// - Sink rate < 2 m/s
		// - Pitch < 5 deg
		// - Roll < 5 deg	
		// - On runway
		
		bool ok_speed = (m_speed < 80);
		bool ok_sink = (fabs(m_vel.y) < 2);
		bool ok_pitch = (fabs(angs.y) < 5);
		bool ok_roll = (fabs(angs.x) < 5);
		bool ok_runway = (m_pos.x > -m_runway_width) && (m_pos.x < m_runway_width) && (m_pos.z > -m_runway_length) && (m_pos.z < m_runway_length );
		m_landing_status = ok_speed && ok_sink && ok_pitch && ok_roll && ok_runway;		

		sprintf ( msg, "%s\n Speed (<80): %4.1f m/s     %s\n Sink rate (<2): %4.1f m/s      %s\n Pitch (<5): %4.1f deg     %s\n Roll (<5): %4.1f deg     %s\n On Runway: %s\n", 
				            m_landing_status  ? "LANDED!" : "CRASH", 
							m_speed,		ok_speed ? "OK" : "FAIL", 
							m_vel.y,		ok_sink ? "OK" : "FAIL", 
							fabs(angs.y),	ok_pitch ? "OK" : "FAIL",
							fabs(angs.x),	ok_roll ? "OK" : "FAIL",
				            ok_runway ? "Yes     OK" : "No     FAIL" );	

 		m_landing_info = msg;			
	}	
	m_airborn = 0;
}


void Sample::Advance ()
{
	Vec3F force, torque;
	
	float m_LiftFactor = 0.0001;
	float m_DragFactor = 0.0001;

	float mass = 0.1;	// body mass (kg)
	
	// Body frame of reference
	Vec3F fwd = Vec3F(1,0,0) * m_orient;		// X-axis is body forward
	Vec3F up  = Vec3F(0,1,0) * m_orient;		// Y-axis is body up
	Vec3F right = Vec3F(0,0,1) * m_orient;		// Z-axis is body right

	// Velocity limit
	m_speed = m_vel.Length();
	Vec3F vaxis = m_vel / m_speed;	
	if ( m_speed < 0 ) m_speed =  0;		// planes dont go in reverse
	if ( m_speed > m_max_speed ) m_speed = m_max_speed;
	if ( m_speed==0) vaxis = fwd;

	// Pitch inputs - modify direction of target velocity 
	Quaternion ctrl_pitch;
	if ( m_pos.y <= 0 ) m_pitch_adv = 1.1;
	m_pitch_adv = m_pitch_adv * 0.9995 + m_pitch * 0.005;
	ctrl_pitch.fromAngleAxis ( m_pitch_adv*0.0001, Vec3F(0,0,1) * m_orient );
	vaxis *= ctrl_pitch;	vaxis.Normalize();	

	m_vel = vaxis * m_speed;


	m_force = 0;
	torque = 0;

	// Flaps
	float flap_lift = m_flaps * cos(m_speed/m_max_speed * (PI/2.0) );	// flap lift decreases with speed
	float wing_area = 1 + m_flaps;										// flap increases wing area (drag)

	// Dynamic pressure		
	float airflow = m_speed + m_wind.Dot ( vaxis*-1.0f );		// airflow = aircraft speed + wind over wing
	float p = 1.225;											// air density, kg/m^3
	float dynamic_pressure = 0.5f * p * airflow * airflow;

	// Lift force
	m_aoa = acos( fwd.Dot( vaxis ) )*RADtoDEG + 1;				// angle-of-attack = angle between velocity and body forward		
	if (isnan(m_aoa)) m_aoa = 1;
	float CL = sin( m_aoa * 0.2) + flap_lift;					// CL = coeff of lift, approximate CL curve with sin
	float L = CL * dynamic_pressure * m_LiftFactor * 0.5;		// lift equation. L = CL (1/2 p v^2) A
	m_lift = up * L;
	m_force += m_lift;	

	// Drag force	
	m_drag = vaxis * dynamic_pressure * m_DragFactor * -1.0f * wing_area;	// drag equation. D = Cd (1/2 p v^2) A
	m_force += m_drag; 

	// Thrust force
	m_thrust = fwd * m_power;
	m_force += m_thrust;

	// Update Orientation
	// Directional stability: airplane will typically reorient toward the velocity vector
	//  see: https://en.wikipedia.org/wiki/Directional_stability
	// this is an assumption yet much simpler/faster than integrating body orientation
	// this way we dont need torque, angular vel, or rotational inertia.
	// stalls are possible but not flat spins or 3D flying
	Quaternion angvel;
	angvel.fromRotationFromTo ( fwd, vaxis, 0.001 );
	if ( !isnan(angvel.X) ) {
		m_orient *= angvel;
		m_orient.normalize();
	}	

	// Roll inputs - modify body orientation along X-axis
	Quaternion ctrl_roll;
	ctrl_roll.fromAngleAxis ( m_roll*0.001, Vec3F(1,0,0) * m_orient );
	m_orient *= ctrl_roll; m_orient.normalize();		// roll inputs

	// Integrate position		
	m_accel = m_force / mass;			// body forces
	
	m_accel += Vec3F(0,-9.8,0);		// gravity

	m_accel += m_wind * p * 0.1f;		// wind force. Fw = w^2 p * A, where w=wind speed, p=air density, A=frontal area
	
	m_pos += m_vel * m_DT;

	// Ground condition
	if (m_pos.y <= 0.00001 ) { 

		// Record landing status
		CheckLanding ();		
		
		// Ground forces
		m_pos.y = 0; m_vel.y = 0; 
		m_accel += Vec3F(0,9.8,0);	// ground force (upward)
		m_vel *= 0.9999;				// ground friction
		m_orient.fromDirectionAndRoll ( Vec3F(fwd.x, 0, fwd.z), 0 );	// zero pitch & roll
		ctrl_roll.fromAngleAxis ( -m_roll*0.001, Vec3F(0,1,0) );		// on ground, left/right is rudder
		m_orient *= ctrl_roll; m_orient.normalize();
		m_vel *= ctrl_roll;			

	} else {
		m_airborn++;
		if (m_airborn > 3200) m_landing_info = "";
	}
	
	// integrate velocity
	m_vel += m_accel * m_DT;
	
}



void Sample::CameraToCockpit()
{
	// View direction	
	Vec3F fwd = m_vel; fwd.Normalize();
	Vec3F angs;
	m_orient.toEuler ( angs );

	// Set eye level above centerline
	Vec3F p = m_pos + Vec3F(0,2,0);	  
	
	m_cam->setDirection ( p, p + fwd, -angs.x );
}

void Sample::display ()
{	
	Vec3F pnt;
	Vec4F clr;

	int w = getWidth();
	int h = getHeight();

	if (m_run) { 		
		Advance ();
	}

	if (m_flightcam) {
		CameraToCockpit();
	} else {
		m_cam->SetOrbit ( m_cam->getAng(), m_pos, m_cam->getOrbitDist(), m_cam->getDolly() );
	}
	
	char msg[2048];

	clearGL();
	
	start2D( getWidth(), getHeight() );		

		Vec3F angs;
		m_orient.toEuler ( angs );

		// Instrument display
		sprintf ( msg, "INPUT:     LFT/RIGHT = Ailerons, UP/DOWN = Elevators, W/S keys = THROTTLE, F = FLAPS"); drawText( Vec2F(10, 20), msg, Vec4F(1,1,1,1) );

		sprintf ( msg, "INSTRUMENTS:");										drawText( Vec2F(10, 60), msg, Vec4F(1,1,1,1) );
		sprintf ( msg, "Speed:     %4.3f m/s, %4.1f kph, %4.1f mph", m_speed, m_speed*3.6, m_speed*2.237 ); drawText( Vec2F(10, 80), msg, Vec4F(1,1,1,1) );
		sprintf ( msg, "Power:     %4.1f", m_power );			drawText( Vec2F(10, 100), msg, Vec4F(1,1,1,1) );
		sprintf ( msg, "Altitude:  %4.2f m", m_pos.y );		drawText( Vec2F(10, 120), msg, Vec4F(1,1,1,1) );
		sprintf ( msg, "Sink rate: %4.2f m/s", m_vel.y );	drawText( Vec2F(10, 140), msg, Vec4F(1,1,1,1) );
		sprintf ( msg, "AOA:       %4.4f", m_aoa );			  drawText( Vec2F(10, 160), msg, Vec4F(1,1,1,1) );
		sprintf ( msg, "Roll:      %4.1f", angs.x );		  drawText( Vec2F(10, 180), msg, Vec4F(1,1,1,1) );
		sprintf ( msg, "Pitch:     %4.1f", angs.y );		  drawText( Vec2F(10, 200), msg, Vec4F(1,1,1,1) );
		sprintf ( msg, "Heading:   %4.1f", angs.z );		  drawText( Vec2F(10, 220), msg, Vec4F(1,1,1,1) );
		sprintf ( msg, "Flaps:     %1.0f", m_flaps );		  drawText( Vec2F(10, 240), msg, Vec4F(1,1,1,1) );

		Vec3F lclr;
		lclr = (m_landing_status) ? Vec3F(0,1,0) : Vec3F(1,0,0);
		sprintf ( msg, "%s", m_landing_info.c_str() );		drawText( Vec2F(10, 280), msg, Vec4F(lclr.x,lclr.y,lclr.z,1) );

		/*sprintf ( msg, "Up/Down    Elevators", m_flaps );	drawText(10, 220, msg, 1,1,1,1);
		sprintf ( msg, "Left/Right Ailerons", m_flaps );	drawText(10, 240, msg, 1,1,1,1);
		sprintf ( msg, "Q/A        Power", m_flaps );		drawText(10, 260, msg, 1,1,1,1);
		sprintf ( msg, "F          Flaps", m_flaps );		drawText(10, 280, msg, 1,1,1,1);
		sprintf ( msg, "C          Camera", m_flaps );		drawText(10, 300, msg, 1,1,1,1);*/

	end2D();


	start3D(m_cam);

		// Draw ground
		drawGrid( (m_flightcam) ? Vec4F(1,1,1,1) : Vec4F(1,1,1,.5) );

		// Draw plane forces (orbit cam only)
		if ( !m_flightcam ) {
			Vec3F grav (0,-9.8 * (m_pos.y>0),0);
			Vec3F a,b,c;
			a = Vec3F(1,0,0)*m_orient;
			b = Vec3F(0,1,0)*m_orient;
			c = Vec3F(0,0,1)*m_orient;			
			drawLine3D ( m_pos-c, m_pos+c, Vec4F(1,1,1,0.3) );			
			drawLine3D ( m_pos, m_pos+m_lift, Vec4F(0,1,0,1) );
			drawLine3D ( m_pos, m_pos+m_thrust, Vec4F(1,0,0,1) );
			drawLine3D ( m_pos, m_pos+m_drag, Vec4F(1,0,1,1) );			
			drawLine3D ( m_pos, m_pos+m_force, Vec4F(0,1,1,.2) );
			drawLine3D ( m_pos, m_pos + grav*0.1f, Vec4F(0.5,0.5,0.8,1) );
			drawLine3D ( m_pos+Vec3F(0,-.1,0), m_pos + m_vel*0.05f +Vec3F(0,-.1,0), Vec4F(1,1,0,.5) );
			drawLine3D ( m_pos, Vec3F(m_pos.x, 0, m_pos.z), Vec4F(0.5,0.5,0.8,.3) );
			
			drawLine3D ( Vec3F(0,0,0), c, Vec4F(1,1,1,1) );
			drawLine3D ( Vec3F(0,0,0), m_lift, Vec4F(0,1,0,1) );
			drawLine3D ( Vec3F(0,0,0), m_thrust, Vec4F(1,0,0,1) );
			drawLine3D ( Vec3F(0,0,0), m_drag, Vec4F(1,0,1,1) );			
			drawLine3D ( Vec3F(0,0,0), m_force, Vec4F(0,1,1,.2) );
			
			
		}

	end3D();

	drawAll ();
	
	appPostRedisplay();								// Post redisplay since simulation is continuous
}


void Sample::mouse(AppEnum button, AppEnum state, int mods, int x, int y)
{
	int w = getWidth(), h = getHeight();				// window width & height

	mouse_down = (state == AppEnum::BUTTON_PRESS) ? button : -1;

	if (mouse_down == AppEnum::BUTTON_LEFT) {
		
	}
}


void Sample::motion (AppEnum button, int x, int y, int dx, int dy) 
{
	// Get camera for scene
	bool shift = (getMods() & KMOD_SHIFT);		// Shift-key to modify light
	float fine = 0.5f;
	Vec3F dang; 

	switch ( mouse_down ) {	
	case AppEnum::BUTTON_LEFT:  {	
	
		} break;

	case AppEnum::BUTTON_MIDDLE: {
		// Adjust target pos		
		float zoom = (m_cam->getOrbitDist() - m_cam->getDolly()) * 0.0003f;
		m_cam->moveRelative ( float(dx) * zoom, float(-dy) * zoom, 0 );	
		} break; 

	case AppEnum::BUTTON_RIGHT: {
		// Adjust orbit angles
		Vec3F angs = m_cam->getAng();
		angs.x += dx*0.2f;
		angs.y -= dy*0.2f;				
		m_cam->SetOrbit ( angs, m_cam->getToPos(), m_cam->getOrbitDist(), m_cam->getDolly() );
		} break;	

	}
}

void Sample::mousewheel(int delta)
{
	// Adjust zoom
	float zoomamt = 1.0;
	float dist = m_cam->getOrbitDist();
	float dolly = m_cam->getDolly();
	float zoom = (dist - dolly) * 0.001f;
	dist -= delta * zoom * zoomamt;
	
	m_cam->SetOrbit(m_cam->getAng(), m_cam->getToPos(), dist, dolly);		
}




void Sample::keyboard(int keycode, AppEnum action, int mods, int x, int y)
{
	if (action == AppEnum::BUTTON_RELEASE) {
		switch ( keycode ) {
		case KEY_LEFT: case KEY_RIGHT:  m_roll = 0; break;
		case KEY_UP: case KEY_DOWN:		m_pitch = 0; break;
		};
		return;
	}

	switch ( keycode ) {
	case ' ':	m_run = !m_run;	break;
	case 'c':	
		m_flightcam = !m_flightcam; 		
		if (!m_flightcam)
			m_cam->SetOrbit ( Vec3F(-30,30,0), m_pos, m_cam->getOrbitDist(), m_cam->getDolly() );
		break;
	case 'w': case 'q':
		m_power += 0.1;
		if (m_power > 10) m_power = 10;
		break;
	case 's': case 'a':
		m_power -= 0.1; 
		if (m_power < 0) m_power = 0; 
		break;
	case 'f':
		m_flaps = (m_flaps==0) ? 1 : 0;
		break;
	case KEY_LEFT:	m_roll = -1.0; break;
	case KEY_RIGHT:	m_roll = +1.0; break;
	case KEY_UP:	m_pitch = -1.0; break;
	case KEY_DOWN:	m_pitch = +1.0; break;
	};
}

void Sample::reshape (int w, int h)
{
	glViewport ( 0, 0, w, h );
	setview2D ( w, h );

	m_cam->setAspect(float(w) / float(h));
	m_cam->SetOrbit(m_cam->getAng(), m_cam->getToPos(), m_cam->getOrbitDist(), m_cam->getDolly());	
		
	appPostRedisplay();	
}

void Sample::startup ()
{
	int w = 1900, h = 1000;
	appStart ( "Flightsim (c) Rama Karl, 2023. MIT license", "Flight simulation", w, h, 4, 2, 16, false );
}

void Sample::shutdown()
{
}


