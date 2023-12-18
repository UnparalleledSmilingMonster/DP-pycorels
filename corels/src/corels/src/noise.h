#ifndef NOISE_H
#define NOISE_H

#include <boost/random/uniform_01.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/random.hpp>
#include <memory>


class Noise{


private:
    double epsilon;
    double delta;
    double global_sensitivity;
    double b_lap;
    //RAII : Ressource Acquisition Is Initialization ?
    boost::mt19937 rng;
    boost::random::uniform_01<double> dist01;
    typedef boost::random::variate_generator<boost::mt19937&, boost::random::uniform_01<> > uniform_generator;
    std::unique_ptr<uniform_generator> uniform01 = nullptr; //we create a unique pointer to make sure they are no other pointers allocated to the same generator


public :
    Noise(double epsi, double delt, double global_sens, unsigned int seed);

    double laplace_noise();
    double get_epsilon() const {return this->epsilon;}
    double get_delta() const {return this->delta;}
    double get_sensitivity() const {return this->global_sensitivity;}

};

#endif
