//
// Created by Carmen C on 25/3/2020.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_EMD_H_
#define BULLDOG_MODULES_ENGINE_SRC_EMD_H_

struct sSignature{
  int n;
  unsigned short *features_;
  const double *weights_;
};

struct sFlow{
  int from_;
  int to_;
  double amount_;
};

struct sNode1 {
  int i_;
  double val_;
  struct sNode1 *next_;
};

struct sNode2 {
  int i_;
  int j_;
  double val_;
  struct sNode2 *next_c_;               /* NEXT COLUMN */
  struct sNode2 *next_r_;               /* NEXT ROW */
};

class EMD {
 public:
  EMD(const double *w1, const double *w2);
  ~EMD();
  double Compute(sFlow *flow, int *flow_size);
 private:
  int max_sig_size_;
  int max_iterations_;
  double emd_infinity_;
  double epsilon_;
  sSignature sig1_;
  sSignature sig2_;

  int n1_;
  int n2_;
  double **c_;
  sNode2 *x_;
  sNode2 *end_x_;
  sNode2 *enter_x_;
  char **is_x_;
  sNode2 **rows_x_;
  sNode2 **cols_x_;
  double max_w_;
  double max_c_;

  double Init();
  void FindBasicVariables(sNode1 *U, sNode1 *V);
  int IsOptimal(sNode1 *U, sNode1 *V);
  int FindLoop(sNode2 **Loop);
  void NewSolution();
  void Russel(double *S, double *D);
  void AddBasicVariable(int minI, int minJ, double *S, double *D,
                        sNode1 *PrevUMinI, sNode1 *PrevVMinJ,
                        sNode1 *UHead);
  static double GroundDist(unsigned short f1, unsigned short f2) {
    return abs((float)(f1-f2));
  }
};

#endif //BULLDOG_MODULES_ENGINE_SRC_EMD_H_
