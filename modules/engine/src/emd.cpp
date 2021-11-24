//
// Created by Carmen C on 25/3/2020.
//

#include <bulldog/logger.hpp>
#include "emd.h"
#include "bucket.h"

static unsigned short HISTOGRAM_X[HISTOGRAM_SIZE] =
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
        11, 12, 13, 14, 15, 16, 17, 18,
        19, 20, 21, 22, 23, 24, 25, 26,
        27, 28, 29, 30, 31, 32, 33, 34,
        35, 36, 37, 38, 39, 40, 41, 42,
        43, 44, 45, 46, 47, 48, 49};

EMD::EMD(const double *w1, const double *w2) {
  max_sig_size_ = 100;
  max_iterations_ = 500;
  emd_infinity_ = 1e20;
  epsilon_ = 1e-6;
  sig1_ = {HISTOGRAM_SIZE, HISTOGRAM_X, w1};
  sig2_ = {HISTOGRAM_SIZE, HISTOGRAM_X, w2};

  c_ = new double *[max_sig_size_ + 1];
  is_x_ = new char *[max_sig_size_ + 1];
  for (int i = 0; i < max_sig_size_ + 1; i++) {
    c_[i] = new double[max_sig_size_ + 1];
    is_x_[i] = new char[max_sig_size_ + 1];
  }

  x_ = new sNode2[(max_sig_size_ + 1) * 2];
  rows_x_ = new sNode2 *[(max_sig_size_ + 1)];
  cols_x_ = new sNode2 *[(max_sig_size_ + 1)];
}

EMD::~EMD() {
  for (int i = 0; i < max_sig_size_ + 1; i++) {
    delete[] c_[i];
    delete[] is_x_[i];
  }
  delete[] c_;
  delete[] is_x_;
  delete[] x_;
  delete[] rows_x_;
  delete[] cols_x_;
}

double EMD::Compute(sFlow *flow, int *flow_size) {
  int itr;
  double totalCost;
  double w;
  sNode2 *XP;
  sFlow *FlowP;
  sNode1 U[max_sig_size_ + 1], V[max_sig_size_ + 1];

  w = Init();

  if (n1_ > 1 && n2_ > 1)  /* IF n1_ = 1 OR n2_ = 1 THEN WE ARE DONE */
  {
    for (itr = 1; itr < max_iterations_; itr++) {
      /* FIND BASIC VARIABLES */
      FindBasicVariables(U, V);

      /* CHECK FOR OPTIMALITY */
      if (IsOptimal(U, V))
        break;

      /* IMPROVE SOLUTION */
      NewSolution();
    }
    if (itr == max_iterations_)
      logger::error("emd: Maximum number of iterations has been reached (%d)", max_iterations_);
  }

  /* COMPUTE THE TOTAL FLOW */
  totalCost = 0;
  if (flow != NULL)
    FlowP = flow;
  for (XP = x_; XP < end_x_; XP++) {
    if (XP == enter_x_)  /* enter_x_ IS THE EMPTY SLOT */
      continue;
    if (XP->i_ == sig1_.n || XP->j_ == sig2_.n)  /* DUMMY FEATURE */
      continue;
    if (XP->val_ == 0)  /* ZERO FLOW */
      continue;

    totalCost += (double) XP->val_ * c_[XP->i_][XP->j_];

    if (flow != NULL) {
      FlowP->from_ = XP->i_;
      FlowP->to_ = XP->j_;
      FlowP->amount_ = XP->val_;
      FlowP++;
    }
  }
  if (flow != NULL)
    *flow_size = FlowP - flow;

  /* RETURN THE NORMALIZED COST == EMD */
  return (double) (totalCost / w);
}
void EMD::Russel(double *S, double *D) {
  int i, j, found, minI, minJ;
  double deltaMin, oldVal, diff;
  double Delta[max_sig_size_ + 1][max_sig_size_ + 1];
  sNode1 Ur[max_sig_size_ + 1], Vr[max_sig_size_ + 1];
  sNode1 uHead, *CurU, *PrevU;
  sNode1 vHead, *CurV, *PrevV;
  sNode1 *PrevUMinI, *PrevVMinJ, *Remember;

  /* INITIALIZE THE ROWS LIST (Ur), AND THE COLUMNS LIST (Vr) */
  uHead.next_ = CurU = Ur;
  for (i = 0; i < n1_; i++) {
    CurU->i_ = i;
    CurU->val_ = -emd_infinity_;
    CurU->next_ = CurU + 1;
    CurU++;
  }
  (--CurU)->next_ = NULL;

  vHead.next_ = CurV = Vr;
  for (j = 0; j < n2_; j++) {
    CurV->i_ = j;
    CurV->val_ = -emd_infinity_;
    CurV->next_ = CurV + 1;
    CurV++;
  }
  (--CurV)->next_ = NULL;

  /* FIND THE MAXIMUM ROW AND COLUMN VALUES (Ur[i] AND Vr[j]) */
  for (i = 0; i < n1_; i++)
    for (j = 0; j < n2_; j++) {
      double v;
      v = c_[i][j];
      if (Ur[i].val_ <= v)
        Ur[i].val_ = v;
      if (Vr[j].val_ <= v)
        Vr[j].val_ = v;
    }

  /* COMPUTE THE Delta MATRIX */
  for (i = 0; i < n1_; i++)
    for (j = 0; j < n2_; j++)
      Delta[i][j] = c_[i][j] - Ur[i].val_ - Vr[j].val_;

  /* FIND THE BASIC VARIABLES */
  do {
    /* FIND THE SMALLEST Delta[i][j] */
    found = 0;
    deltaMin = emd_infinity_;
    PrevU = &uHead;
    for (CurU = uHead.next_; CurU != NULL; CurU = CurU->next_) {
      int i;
      i = CurU->i_;
      PrevV = &vHead;
      for (CurV = vHead.next_; CurV != NULL; CurV = CurV->next_) {
        int j;
        j = CurV->i_;
        if (deltaMin > Delta[i][j]) {
          deltaMin = Delta[i][j];
          minI = i;
          minJ = j;
          PrevUMinI = PrevU;
          PrevVMinJ = PrevV;
          found = 1;
        }
        PrevV = CurV;
      }
      PrevU = CurU;
    }

    if (!found)
      break;

    /* ADD X[minI][minJ] TO THE BASIS, AND ADJUST SUPPLIES AND COST */
    Remember = PrevUMinI->next_;
    AddBasicVariable(minI, minJ, S, D, PrevUMinI, PrevVMinJ, &uHead);

    /* UPDATE THE NECESSARY Delta[][] */
    if (Remember == PrevUMinI->next_)  /* LINE minI WAS DELETED */
    {
      for (CurV = vHead.next_; CurV != NULL; CurV = CurV->next_) {
        int j;
        j = CurV->i_;
        if (CurV->val_ == c_[minI][j])  /* COLUMN j NEEDS UPDATING */
        {
          /* FIND THE NEW MAXIMUM VALUE IN THE COLUMN */
          oldVal = CurV->val_;
          CurV->val_ = -emd_infinity_;
          for (CurU = uHead.next_; CurU != NULL; CurU = CurU->next_) {
            int i;
            i = CurU->i_;
            if (CurV->val_ <= c_[i][j])
              CurV->val_ = c_[i][j];
          }

          /* IF NEEDED, ADJUST THE RELEVANT Delta[*][j] */
          diff = oldVal - CurV->val_;
          if (fabs(diff) < epsilon_ * max_c_)
            for (CurU = uHead.next_; CurU != NULL; CurU = CurU->next_)
              Delta[CurU->i_][j] += diff;
        }
      }
    } else  /* COLUMN minJ WAS DELETED */
    {
      for (CurU = uHead.next_; CurU != NULL; CurU = CurU->next_) {
        int i;
        i = CurU->i_;
        if (CurU->val_ == c_[i][minJ])  /* ROW i NEEDS UPDATING */
        {
          /* FIND THE NEW MAXIMUM VALUE IN THE ROW */
          oldVal = CurU->val_;
          CurU->val_ = -emd_infinity_;
          for (CurV = vHead.next_; CurV != NULL; CurV = CurV->next_) {
            int j;
            j = CurV->i_;
            if (CurU->val_ <= c_[i][j])
              CurU->val_ = c_[i][j];
          }

          /* If NEEDED, ADJUST THE RELEVANT Delta[i][*] */
          diff = oldVal - CurU->val_;
          if (fabs(diff) < epsilon_ * max_c_)
            for (CurV = vHead.next_; CurV != NULL; CurV = CurV->next_)
              Delta[i][CurV->i_] += diff;
        }
      }
    }
  } while (uHead.next_ != NULL || vHead.next_ != NULL);
}
void EMD::AddBasicVariable(int minI, int minJ, double *S, double *D,
                           sNode1 *PrevUMinI, sNode1 *PrevVMinJ,
                           sNode1 *UHead) {
  double T;

  if (fabs(S[minI] - D[minJ]) <= epsilon_ * max_w_)  /* DEGENERATE CASE */
  {
    T = S[minI];
    S[minI] = 0;
    D[minJ] -= T;
  } else if (S[minI] < D[minJ])  /* SUPPLY EXHAUSTED */
  {
    T = S[minI];
    S[minI] = 0;
    D[minJ] -= T;
  } else  /* DEMAND EXHAUSTED */
  {
    T = D[minJ];
    D[minJ] = 0;
    S[minI] -= T;
  }

  /* X(minI,minJ) IS A BASIC VARIABLE */
  is_x_[minI][minJ] = 1;

  end_x_->val_ = T;
  end_x_->i_ = minI;
  end_x_->j_ = minJ;
  end_x_->next_c_ = rows_x_[minI];
  end_x_->next_r_ = cols_x_[minJ];
  rows_x_[minI] = end_x_;
  cols_x_[minJ] = end_x_;
  end_x_++;

  /* DELETE SUPPLY ROW ONLY IF THE EMPTY, AND IF NOT LAST ROW */
  if (S[minI] == 0 && UHead->next_->next_ != NULL)
    PrevUMinI->next_ = PrevUMinI->next_->next_;  /* REMOVE ROW FROM LIST */
  else
    PrevVMinJ->next_ = PrevVMinJ->next_->next_;  /* REMOVE COLUMN FROM LIST */
}

void EMD::FindBasicVariables(sNode1 *U, sNode1 *V) {
  int i, j, found;
  int UfoundNum, VfoundNum;
  sNode1 u0Head, u1Head, *CurU, *PrevU;
  sNode1 v0Head, v1Head, *CurV, *PrevV;

  /* INITIALIZE THE ROWS LIST (U) AND THE COLUMNS LIST (V) */
  u0Head.next_ = CurU = U;
  for (i = 0; i < n1_; i++) {
    CurU->i_ = i;
    CurU->next_ = CurU + 1;
    CurU++;
  }
  (--CurU)->next_ = NULL;
  u1Head.next_ = NULL;

  CurV = V + 1;
  v0Head.next_ = n2_ > 1 ? V + 1 : NULL;
  for (j = 1; j < n2_; j++) {
    CurV->i_ = j;
    CurV->next_ = CurV + 1;
    CurV++;
  }
  (--CurV)->next_ = NULL;
  v1Head.next_ = NULL;

  /* THERE ARE n1_+n2_ VARIABLES BUT ONLY n1_+n2_-1 INDEPENDENT EQUATIONS,
     SO SET V[0]=0 */
  V[0].i_ = 0;
  V[0].val_ = 0;
  v1Head.next_ = V;
  v1Head.next_->next_ = NULL;

  /* LOOP UNTIL ALL VARIABLES ARE FOUND */
  UfoundNum = VfoundNum = 0;
  while (UfoundNum < n1_ || VfoundNum < n2_) {
    found = 0;
    if (VfoundNum < n2_) {
      /* LOOP OVER ALL MARKED COLUMNS */
      PrevV = &v1Head;
      for (CurV = v1Head.next_; CurV != NULL; CurV = CurV->next_) {
        j = CurV->i_;
        /* FIND THE VARIABLES IN COLUMN j */
        PrevU = &u0Head;
        for (CurU = u0Head.next_; CurU != NULL; CurU = CurU->next_) {
          i = CurU->i_;
          if (is_x_[i][j]) {
            /* COMPUTE U[i] */
            CurU->val_ = c_[i][j] - CurV->val_;
            /* ...AND ADD IT TO THE MARKED LIST */
            PrevU->next_ = CurU->next_;
            CurU->next_ = u1Head.next_ != NULL ? u1Head.next_ : NULL;
            u1Head.next_ = CurU;
            CurU = PrevU;
          } else
            PrevU = CurU;
        }
        PrevV->next_ = CurV->next_;
        VfoundNum++;
        found = 1;
      }
    }
    if (UfoundNum < n1_) {
      /* LOOP OVER ALL MARKED ROWS */
      PrevU = &u1Head;
      for (CurU = u1Head.next_; CurU != NULL; CurU = CurU->next_) {
        i = CurU->i_;
        /* FIND THE VARIABLES IN ROWS i */
        PrevV = &v0Head;
        for (CurV = v0Head.next_; CurV != NULL; CurV = CurV->next_) {
          j = CurV->i_;
          if (is_x_[i][j]) {
            /* COMPUTE V[j] */
            CurV->val_ = c_[i][j] - CurU->val_;
            /* ...AND ADD IT TO THE MARKED LIST */
            PrevV->next_ = CurV->next_;
            CurV->next_ = v1Head.next_ != NULL ? v1Head.next_ : NULL;
            v1Head.next_ = CurV;
            CurV = PrevV;
          } else
            PrevV = CurV;
        }
        PrevU->next_ = CurU->next_;
        UfoundNum++;
        found = 1;
      }
    }
    if (!found) {
      fprintf(stderr, "emd: Unexpected error in findBasicVariables!\n");
      fprintf(stderr, "This typically happens when the epsilon_ defined in\n");
      fprintf(stderr, "emd.h is not right for the scale of the problem.\n");
      exit(1);
    }
  }
}
int EMD::IsOptimal(sNode1 *U, sNode1 *V) {
  double delta, deltaMin;
  int i, j, minI, minJ;
  minI = 0;
  minJ = 0;

  /* FIND THE MINIMAL Cij-Ui-Vj OVER ALL i,j */
  deltaMin = emd_infinity_;
  for (i = 0; i < n1_; i++)
    for (j = 0; j < n2_; j++)
      if (!is_x_[i][j]) {
        delta = c_[i][j] - U[i].val_ - V[j].val_;
        if (deltaMin > delta) {
          deltaMin = delta;
          minI = i;
          minJ = j;
        }
      }

  if (deltaMin == emd_infinity_) {
    fprintf(stderr, "emd: Unexpected error in isOptimal.\n");
    exit(0);
  }

  enter_x_->i_ = minI;
  enter_x_->j_ = minJ;

  /* IF NO NEGATIVE deltaMin, WE FOUND THE OPTIMAL SOLUTION */
  return deltaMin >= -epsilon_ * max_c_;
}

void EMD::NewSolution() {
  int i, j, k;
  double xMin;
  int steps;
  sNode2 *Loop[2 * (max_sig_size_ + 1)], *CurX, *LeaveX;
  LeaveX = nullptr;

  logger::trace("EnterX = (%d,%d)\n", enter_x_->i_, enter_x_->j_);

  /* ENTER THE NEW BASIC VARIABLE */
  i = enter_x_->i_;
  j = enter_x_->j_;
  is_x_[i][j] = 1;
  enter_x_->next_c_ = rows_x_[i];
  enter_x_->next_r_ = cols_x_[j];
  enter_x_->val_ = 0;
  rows_x_[i] = enter_x_;
  cols_x_[j] = enter_x_;

  /* FIND A CHAIN REACTION */
  steps = FindLoop(Loop);

  /* FIND THE LARGEST VALUE IN THE LOOP */
  xMin = emd_infinity_;
  for (k = 1; k < steps; k += 2) {
    if (Loop[k]->val_ < xMin) {
      LeaveX = Loop[k];
      xMin = Loop[k]->val_;
    }
  }

  /* UPDATE THE LOOP */
  for (k = 0; k < steps; k += 2) {
    Loop[k]->val_ += xMin;
    Loop[k + 1]->val_ -= xMin;
  }

  logger::trace("LeaveX = (%d,%d)\n", LeaveX->i_, LeaveX->j_);

  /* REMOVE THE LEAVING BASIC VARIABLE */
  i = LeaveX->i_;
  j = LeaveX->j_;
  is_x_[i][j] = 0;
  if (rows_x_[i] == LeaveX)
    rows_x_[i] = LeaveX->next_c_;
  else
    for (CurX = rows_x_[i]; CurX != NULL; CurX = CurX->next_c_)
      if (CurX->next_c_ == LeaveX) {
        CurX->next_c_ = CurX->next_c_->next_c_;
        break;
      }
  if (cols_x_[j] == LeaveX)
    cols_x_[j] = LeaveX->next_r_;
  else
    for (CurX = cols_x_[j]; CurX != NULL; CurX = CurX->next_r_)
      if (CurX->next_r_ == LeaveX) {
        CurX->next_r_ = CurX->next_r_->next_r_;
        break;
      }

  /* SET enter_x_ TO BE THE NEW EMPTY SLOT */
  enter_x_ = LeaveX;
}

int EMD::FindLoop(sNode2 **loop) {
  int steps;
  sNode2 **CurX, *NewX;
  char IsUsed[2 * (max_sig_size_ + 1)];

  for (int i = 0; i < n1_ + n2_; i++)
    IsUsed[i] = 0;

  CurX = loop;
  NewX = *CurX = enter_x_;
  IsUsed[enter_x_ - x_] = 1;
  steps = 1;

  do {
    if (steps % 2 == 1) {
      /* FIND AN UNUSED X IN THE ROW */
      NewX = rows_x_[NewX->i_];
      while (NewX != nullptr && IsUsed[NewX - x_])
        NewX = NewX->next_c_;
    } else {
      /* FIND AN UNUSED X IN THE COLUMN, OR THE ENTERING X */
      NewX = cols_x_[NewX->j_];
      while (NewX != nullptr && IsUsed[NewX - x_] && NewX != enter_x_)
        NewX = NewX->next_r_;
      if (NewX == enter_x_)
        break;
    }

    if (NewX != nullptr) {/* FOUND THE NEXT X */
      /* ADD X TO THE LOOP */
      *++CurX = NewX;
      IsUsed[NewX - x_] = 1;
      steps++;
    } else {/* DIDN'T FIND THE NEXT X */
      /* BACKTRACK */
      do {
        NewX = *CurX;
        do {
          if (steps % 2 == 1)
            NewX = NewX->next_r_;
          else
            NewX = NewX->next_c_;
        } while (NewX != nullptr && IsUsed[NewX - x_]);

        if (NewX == nullptr) {
          IsUsed[*CurX - x_] = 0;
          CurX--;
          steps--;
        }
      } while (NewX == nullptr && CurX >= loop);

      IsUsed[*CurX - x_] = 0;
      *CurX = NewX;
      IsUsed[NewX - x_] = 1;
    }
  } while (CurX >= loop);

  if (CurX == loop) {
    logger::error("emd: unexpected error in FindLoop");
    exit(EXIT_FAILURE);
  }

  return steps;
}

double EMD::Init() {
  int i, j;
  double sSum, dSum, diff;
  unsigned short *P1, *P2;
  double S[max_sig_size_ + 1], D[max_sig_size_ + 1];

  n1_ = sig1_.n;
  n2_ = sig2_.n;

  if (n1_ > max_sig_size_ || n2_ > max_sig_size_) {
    fprintf(stderr, "emd: Signature size is limited to %d\n", max_sig_size_);
    exit(1);
  }

  /* COMPUTE THE DISTANCE MATRIX */
  max_c_ = 0;
  for (i = 0, P1 = sig1_.features_; i < n1_; i++, P1++)
    for (j = 0, P2 = sig2_.features_; j < n2_; j++, P2++) {
      c_[i][j] = GroundDist(*P1, *P2);
      if (c_[i][j] > max_c_)
        max_c_ = c_[i][j];
    }

  /* SUM UP THE SUPPLY AND DEMAND */
  sSum = 0.0;
  for (i = 0; i < n1_; i++) {
    S[i] = sig1_.weights_[i];
    sSum += sig1_.weights_[i];
    rows_x_[i] = NULL;
  }
  dSum = 0.0;
  for (j = 0; j < n2_; j++) {
    D[j] = sig2_.weights_[j];
    dSum += sig2_.weights_[j];
    cols_x_[j] = NULL;
  }

  /* IF SUPPLY DIFFERENT THAN THE DEMAND, ADD A ZERO-COST DUMMY CLUSTER */
  diff = sSum - dSum;
  if (fabs(diff) >= epsilon_ * sSum) {
    if (diff < 0.0) {
      for (j = 0; j < n2_; j++)
        c_[n1_][j] = 0;
      S[n1_] = -diff;
      rows_x_[n1_] = NULL;
      n1_++;
    } else {
      for (i = 0; i < n1_; i++)
        c_[i][n2_] = 0;
      D[n2_] = diff;
      cols_x_[n2_] = NULL;
      n2_++;
    }
  }

  /* INITIALIZE THE BASIC VARIABLE STRUCTURES */
  for (i = 0; i < n1_; i++)
    for (j = 0; j < n2_; j++)
      is_x_[i][j] = 0;
  end_x_ = x_;

  max_w_ = sSum > dSum ? sSum : dSum;

  /* FIND INITIAL SOLUTION */
  Russel(S, D);

  enter_x_ = end_x_++;  /* AN EMPTY SLOT (ONLY n1_+n2_-1 BASIC VARIABLES) */

  return sSum > dSum ? dSum : sSum;
}
