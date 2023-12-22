#ifndef NOISE_H
#define NOISE_H

#include <boost/random/uniform_01.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/random.hpp>
#include <memory>


class Noise{


private:
    double epsilon_per_node;
    double delta_per_node;
    double global_sensitivity;
    double b_lap; //the scale coefficient for Laplace mechanism

    std::string method;

    //RAII : Ressource Acquisition Is Initialization ?
    boost::mt19937 rng;
    boost::random::uniform_01<double> dist01;
    typedef boost::random::variate_generator<boost::mt19937&, boost::random::uniform_01<> > uniform_generator;
    std::unique_ptr<uniform_generator> uniform01 = nullptr; //we create a unique pointer to make sure they are no other pointers allocated to the same generator


public :
    Noise(double epsilon, double delta, double global_sens, unsigned int max_length, unsigned int nsamples, unsigned int seed, std::string meth = "global");
    //~Noise();    //Unique ptr automatically get deleted once out of scope


    double laplace_noise();
    double get_epsilon() const {return this->epsilon_per_node;}
    double get_delta() const {return this->delta_per_node;}
    double get_sensitivity() const {return this->global_sensitivity;}

};

#endif
