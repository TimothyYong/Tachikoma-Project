#include "pfilter.h"
#include "sim_robot.h"
#include <cmath>
#include <random>
#include "draw.h"
#include "mathfun.h"

using namespace arma;
using namespace std;

static double erfinv(double p);
static double gaussianNoise(double sigma);
static double gauss(double mu, double sigma2);

pfilter::pfilter(void) {
}

/** This is the constructor for the particle filter (think of it as your init)
 *	@param nparticles the number of particles to create
 *	@param map the pointer to the map (just store it)
 *	@param landmarks a list of landmarks, where each landmark is described in sim_landmark.h
 */
pfilter::pfilter(int nparticles, sim_map *map, vector<sim_landmark> &landmarks,
	double x, double y, double t, double initial_sigma) {
	double x_, y_, t_;
	// STEP 1: store the map and landmark variables
	this->map = map;
	this->landmarks = landmarks;

	// STEP 2: create a bunch of particles, place them into this->particles
	vector<sim_robot> new_particles;
	for (int i = 0; i < nparticles; i++) {
		x_ = x + gaussianNoise(initial_sigma);
		y_ = y + gaussianNoise(initial_sigma);
		t_ = t + gaussianNoise(5.0);
		sim_robot newbot; 
		newbot.set_pose(x_, y_, t_);
		new_particles.push_back(newbot);
	}
	particles.clear();
	particles = new_particles;

	// STEP 3: initialize the health as a vec of ones
	health = ones<vec>(nparticles);
}

pfilter::~pfilter(void) {
}

/** Set the noise for each robot
 *	@param vs the velocity sigma2
 *	@param ws the angular velocity sigma2
 */
void pfilter::set_noise(double vs, double ws) {
	// Set the noise for each robot
	for (sim_robot &bot : this->particles) {
		bot.set_noise(vs, ws);
	}
	this->vs = vs;
	this->ws = ws;
}

/** Set the size for each robot
 *	@param r the radius of the robot
 */
void pfilter::set_size(double r) {
	for (sim_robot &bot : this->particles) {
		bot.set_size(r);
	}
}

/** Move each robot by some velocity and angular velocity
 *	In here, also detect if the robot's position goes out of range,
 *	and handle it
 *	Pretend that the world is circular
 *	@param v the velocity
 *	@param w the angular velocity
 */
void pfilter::move(double vx, double vy, double w) {
	for (sim_robot &bot : this->particles) {
		bot.move(vx, vy, w);
		// circular world!!!!
    bot.x = limit_value(bot.x, 0, (double)this->map->n_cols - 1);
    bot.y = limit_value(bot.y, 0, (double)this->map->n_rows - 1);
	}
}

/** Weigh the "health" of each particle using gaussian error
 *	@param observations a 3xn matrix, where the first row is the x row,
 *											and the second row is the y row
 *	@param health the health vector
 */
void pfilter::weigh(mat &observations) {
	vec theta(observations.n_cols);
	vec radius(observations.n_cols);
	vec R = observations.row(0).t();
	vec T = observations.row(1).t();
	for (int i = 0; i < (int)T.n_cols; i++) {
		T[i] = wrap_value(T[i], -180, 180);
	}
	for (int i = 0; i < particles.size(); i++) {
		sim_robot &particle = particles[i];
		int x = (int)round(particle.x);
		int y = (int)round(particle.y);
		if (x < 0 || x >= (int)map->n_cols ||
				y < 0 || y >= (int)map->n_rows ||
				map->map(y, x) > 0.5) {
			this->health[i] = 0;
			continue;
		}
		for(int j = 0; j < landmarks.size(); j++) { // change this to use observations later
			if (observations(2, j) > 0.5) {
				sim_landmark &landmark = landmarks[j];
				vec diff({ landmark.x - particle.x, landmark.y - particle.y });
				radius[j] = eucdist(diff); // radius of the robot
				theta[j] = wrap_value(angle(diff) - particle.t, -180, 180); // theta of the robot
				this->health[i] *= gauss(abs(R[j] - radius[j]), 10.0);
				this->health[i] *= gauss(wrap_value(T[j] - theta[j], -180, 180), 5.0);
			}
		}
	}
	/*
	for (int i = 0; i < (int)T.n_elem; i++) {
		printf("obs: %.1lf, radius: %.1lf\n", R[i], radius[i]);
	}
	for (int i = 0; i < (int)T.n_elem; i++) {
		printf("obs: %.1lf, theta: %.1lf\n", T[i], theta[i]);
	}
	*/
}

/** Resample all the particles based on the health using the resample wheel
 *	@param health the health of all the particles
 */
void pfilter::resample(void) {
	int N = particles.size();
	vector<sim_robot> p2;
	int index = (int)rand() % N;
	double beta = 0;
	double mw = max(health);
	if (mw == 0) {
		//printf("nothing to resample\n");
		//exit(1);
		return; // nothing to resample
	}
	for (int i = 0; i < N; i++) {
		beta += ((double)rand() / (double)RAND_MAX) * 2 * mw;
		while (beta >= health[index]) {
			beta -= health[index];
			index = (index + 1) % N;
		}
		p2.push_back(particles[index]);
	}
	particles.clear();
	particles = p2;
}

/** Call the weigh and resample functions from here
 *	@param observations the observations of the landmarks
 */
void pfilter::observe(mat observations) {
	if (observations.n_cols > 0) {
	// each column of obs matches to each col of landmarks
		health = ones<vec>(particles.size());
		weigh(observations);
		resample();
	}
}

/** Predict the position and calculate the error of the particle set
 *	@param mu (output) the position ( x, y, theta )
 *	@param sigma (output) the error
 */
void pfilter::predict(vec &mu, mat &sigma) {
	mu = zeros<vec>(3);
	for (int i = 0; i < particles.size(); i++) {
		sim_robot &particle = particles[i];
		mu += vec({ particle.x, particle.y, particle.t });
	}
	mu /= particles.size();
	sigma = zeros<mat>(3, 3);
	// var = (Particle's x_i-Mean)^2 / N 
	for (int i = 0; i < particles.size(); i++) {
		sim_robot &particle = particles[i];
		vec pvec({ particle.x, particle.y, particle.t });
		pvec -= mu;
		sigma += pvec * pvec.t();
	}
	sigma /= particles.size();
}

/** Blit all the particles onto the screen
 *	@param screen the screen to blit the particles onto
 */
void pfilter::blit(cube &screen, int mux, int muy)
{
	int i = 0;
	for (sim_robot &bot : this->particles)
	{
		int x = (int)round(bot.x) - mux + (int)screen.n_cols / 2;
		int y = (int)round(bot.y) - muy + (int)screen.n_rows / 2;
		if (x >= 0 && x < (int)screen.n_cols && y >= 0 && y < (int)screen.n_rows)
		{
			screen(y, x, 0) = 0;
			screen(y, x, 1) = 1;
			screen(y, x, 2) = 0;
		}
		i++;
	}
}

static double erfinv(double p) {
	// approximate maclaurin series (refer to http://mathworld.wolfram.com/InverseErf.html)
	double sqrt_pi = sqrt(M_PI);
	vec a = {
		0.88623,
		0.23201,
		0.12756,
		0.086552
	};
	vec x(a.n_elem);
	for (int i = 0; i < x.n_elem; i++) {
		x(i) = pow(p, 2 * i + 1);
	}
	return dot(a,x);
}

static double gaussianNoise(double sigma) {
	double p = (double)rand() / ((double)RAND_MAX / 2) - 1;
	return erfinv(p) * sigma;
}

static double gauss(double mu, double sigma2) {
	return 1 / sqrt(2 * M_PI * sigma2) * exp(-0.5 * (mu * mu) / sigma2);
}
