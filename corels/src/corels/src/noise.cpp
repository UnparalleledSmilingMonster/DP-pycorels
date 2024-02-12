#ifndef NOISE_CPP
#define NOISE_CPP
#include "noise.h"

#include <boost/math/special_functions/log1p.hpp>
#include <boost/math/special_functions/sign.hpp>
#include <iostream>


//Constructor :
Noise::Noise(double epsilon, double delta, double global_sens, unsigned int max_length, unsigned int nsamples, unsigned int seed, std::string meth):
epsilon_per_node(epsilon/(2*max_length-1)), delta_per_node(delta/(2*max_length-1)), global_sensitivity(global_sens), b_lap(global_sensitivity/epsilon_per_node), rng(boost::mt19937(seed))
{
    method = meth;
    if (method =="global") delta_per_node = 0;
    else if (method == "smooth"){
        throw std::invalid_argument("The smooth sensitivity method is not yet implemented.");
        /*delta = (delta <= 0 )? (double) 1/(nsamples*nsamples*(2*max_length-1)) : delta/(3*(max_length-1));
        b_lap = ; //TODO : Find smooth sensitivity of lower bound : good luck.
        */
    }
    else throw std::invalid_argument("No such method. Choose between global and smooth.");
    uniform01 =std::unique_ptr<uniform_generator>(new uniform_generator(rng, dist01));
}


double Noise::laplace_noise(){
    /*TODO:Generates Laplace noise from inverse transform sampling*/
    double u = uniform01->operator()() -0.5; //Sample u uniform in [-1/2, 1/2]
    return -b_lap*boost::math::sign(u)*boost::math::log1p(1-2*std::abs(u));
}


#endif
