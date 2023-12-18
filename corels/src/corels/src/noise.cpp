#ifndef NOISE_CPP
#define NOISE_CPP
#include "noise.h"

#include <boost/math/special_functions/log1p.hpp>
#include <boost/math/special_functions/sign.hpp>
#include <iostream>


//Constructor :
Noise::Noise(double epsi, double delt, double global_sens, unsigned int seed):
epsilon(epsi), delta(delt), global_sensitivity(global_sens), b_lap(global_sensitivity/epsilon), rng(boost::mt19937(seed))
{
    uniform01 =std::unique_ptr<uniform_generator>(new uniform_generator(rng, dist01));

}


double Noise::laplace_noise(){
    /*TODO:Generates Laplace noise from inverse transform sampling*/
    double u = uniform01->operator()() -0.5; //Sample u uniform in [-1/2, 1/2]
    return -b_lap*boost::math::sign(u)*boost::math::log1p(1-2*std::abs(u));
}


#endif
