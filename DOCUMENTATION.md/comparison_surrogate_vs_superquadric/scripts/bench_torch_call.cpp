// Micro-benchmark: cost of one surrogate wall-contact geometry evaluation (LibTorch forward,
// batch 1, float32) vs the exact analytic part alone. Same calls as surrogate_contact.h.
#include <torch/script.h>
#include <ATen/Parallel.h>
#include <chrono>
#include <cstdio>
#include "surrogate_interface.h"
using namespace LAMMPS_NS::SurrogateInterface;
int main(int argc,char**argv){
  int threads = argc>1 ? atoi(argv[1]) : 0;
  if(threads>0) at::set_num_threads(threads);
  auto m=torch::jit::load("/home/su_estupinan@private.list.lu/SpaceLights/LIGGGHTS-PUBLIC/SS_models/ss-cube-walls/model.pt");
  double q[4]={0.9,0.3,0.2,0.1}; normalize_quaternion(q); double d[3]={0,0,-1}, cp[3];
  const int N=20000; double acc=0;
  for(int i=0;i<500;i++){ torch::NoGradGuard g; acc+=m.forward({torch::tensor({0.8f}),torch::tensor({(float)q[0],(float)q[1],(float)q[2],(float)q[3]}).reshape({1,4})}).toTensor().item<double>(); }
  auto t0=std::chrono::steady_clock::now();
  for(int i=0;i<N;i++){
    torch::NoGradGuard g;
    float z=0.80f+1e-6f*(i%100);
    auto pz=torch::tensor({z},torch::kFloat32);
    auto qt=torch::tensor({(float)q[0],(float)q[1],(float)q[2],(float)q[3]},torch::kFloat32).reshape({1,4});
    std::vector<torch::jit::IValue> in{pz,qt};
    double raw=m.forward(in).toTensor().reshape({-1})[0].item<double>();
    double h=h_exact_cube(q,d); double sd=denormalize_signed_distance(raw,0.7425,0.1314,h);
    contact_point_cube(q,d,sd,cp); acc+=sd;
  }
  auto t1=std::chrono::steady_clock::now();
  for(int i=0;i<N*50;i++){ double h=h_exact_cube(q,d); contact_point_cube(q,d,-1e-3,cp); acc+=h+cp[0]; }
  auto t2=std::chrono::steady_clock::now();
  double us=std::chrono::duration<double,std::micro>(t1-t0).count()/N;
  double us2=std::chrono::duration<double,std::micro>(t2-t1).count()/(N*50);
  printf("threads=%d  surrogate query (torch forward + h_exact + contact point): %.2f us/call   analytic part only: %.4f us/call   (acc=%g)\n",at::get_num_threads(),us,us2,acc);
}
