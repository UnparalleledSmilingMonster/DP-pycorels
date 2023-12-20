#ifndef NOISE_CPP
#define NOISE_CPP
#include "noise.h"

#include <boost/math/special_functions/log1p.hpp>
#include <boost/math/special_functions/sign.hpp>
#include <iostream>


//Constructor :
Noise::Noise(double epsi, double delt, double global_sens, unsigned int nsamples, unsigned int seed, std::string meth):
epsilon(epsi), delta(delt), global_sensitivity(global_sens), b_lap(global_sensitivity/epsilon), rng(boost::mt19937(seed))
{
    method = meth;
    if (method =="global") delta = 0;
    else if (method == "smooth"){
        throw std::invalid_argument("The smooth sensitivity method is not yet implemented.");
        /*delta = (delta <= 0 )? (double) (1/nsamples*nsamples) : delta;
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
