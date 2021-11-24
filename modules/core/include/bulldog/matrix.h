//
// Created by Isaac Zhang on 4/6/20.
//

#ifndef BULLDOG_MODULES_CORE_INCLUDE_BULLDOG_MATRIX_H_
#define BULLDOG_MODULES_CORE_INCLUDE_BULLDOG_MATRIX_H_

#include <iostream>
#include <sstream>
#include<math.h>

using namespace std;

/*
    Todo:
    - Complete error exceptions
    - Importing global functions to class object
    - Optimizations of global functions and class members
    - Data type support tests and improvements
    - Adding array initialization for = operator
    - Adding search operator that returns array of indexes
*/

template<class T, int R = 0, int C = 0>
class Matrix {
 private:
  T *arr;
  class Proxy {
   public:
    Proxy(T *_arr, int width, int x) : _arr(_arr), width(width), x(x) {}
    T &operator[](int y) {
      return _arr[y + width * x];
    }
    T &operator[](const char *key) {
      int y = -1;
      for (int i = 0; i < width; i++) {
        if (_arr[i] == key) {
          y = i;
        }
      }
      if (y == -1) {
        throw "Error: No result fount by " + string(key) + " key in the array.";
      }
      return _arr[y + width * x];
    }
   private:
    T *_arr;
    int width, x;
  };
  class Dump {
   private:
    Matrix *m;
    template<class V>
    string toString(V var) {
      ostringstream sstr;
      sstr << var;
      return sstr.str();
    }
   public:
    Dump(Matrix *m) : m(m) {}
    void array() {
      cout << "Array view: " << endl;
      for (int i = 0; i < m->width * m->height; i++) {
        cout << "arr[" << i << "]: " << m->arr[i] << endl;
      }
    }
    void table() {
      cout << endl << "Table view: " << endl;
      int column[m->width];
      int length = 0;
      for (int i = 0; i < m->width; i++) {
        cout << m->arr[i] << "            ";
        column[i] = toString(m->arr[i]).length() + 12;
        length += column[i];
      }
      cout << endl;
      for (int i = 0; i < length; i++) {
        cout << "-";
      }
      cout << endl;
      for (int i = m->width; i < m->width * m->height; i++) {
        int border = toString(m->arr[i]).length();
        bool etc = false;
        if (border > column[i % m->width])
          etc = true;

        for (int j = 0; j < column[i % m->width]; j++) {
          if (j > border) {
            cout << " ";
          } else {
            if (j < (column[i % m->width] - (etc ? 3 : 0)))
              cout << toString(m->arr[i])[j];
            else
              cout << ".";
          }
        }
        cout << " ";
        if ((i + 1) % m->width == 0)
          cout << endl;
      }

    }
    void matrix() {
      cout << endl << "Matrix view: " << endl;
      int column[m->width];
      int length = 0;
      int space;
      for (int i = 0; i < m->width; i++) {
        space = sizeof(m->arr[i]);
        column[i] = toString(m->arr[i]).length() + (space > 12 ? space / 2 : space);
        length += column[i];
      }
      cout << "-";
      for (int i = 0; i < length + 2; i++) {
        cout << " ";
      }
      cout << "-";
      cout << endl;
      for (int i = 0; i < m->width * m->height; i++) {
        if (i % m->width == 0)
          cout << "| ";
        int border = toString(m->arr[i]).length();
        bool etc = false;
        if (border > column[i % m->width])
          etc = true;

        for (int j = 0; j < column[i % m->width]; j++) {
          if (j > border) {
            cout << " ";
          } else {
            if (j < (column[i % m->width] - (etc ? 4 : 0)))
              cout << toString(m->arr[i])[j];
            else {
              cout << "...";
              break;
            }
          }
        }
        cout << " ";
        if ((i + 1) % m->width == 0) {
          cout << " |";
          cout << endl;
        }
      }
      cout << "-";
      for (int i = 0; i < length + 2; i++) {
        cout << " ";
      }
      cout << "-";
      cout << endl;
    }
  };

 public:
  int width, height;
  static const int isMatrix = 10;
  Dump dump = Dump(this);
  Matrix(int rows = 0, int columns = 0) {
    if (rows > 0 && columns > 0) {
      height = rows;
      width = columns;
    } else if (R > 0 && C > 0) {
      height = R;
      width = C;
    } else {
      throw "Matrix initialization should have height and width arguments and must be greater than zero.";
    }
    arr = new T[height * width];
  }

  Proxy operator[](int x) {
    return Proxy(arr, width, x);
  }
  bool operator==(Matrix m) {
    if (this->width == m.width && this->height == m.height) {
      for (int i = 0; i < this->width; i++) {
        for (int j = 0; j < this->height; j++) {
          if ((*this)[i][j] != m[i][j])
            return false;
        }
      }
      return true;
    }
    return false;
  }
  bool operator!=(Matrix m) {
    return !((*this) == m);
  }
  bool operator<(Matrix m) {
    if (this->width == m.width && this->height == m.height) {
      for (int i = 0; i < this->width; i++) {
        for (int j = 0; j < this->height; j++) {
          if ((*this)[i][j] >= m[i][j])
            return false;
        }
      }
      return true;
    } else if (this->width * this->height < m.width * m.height) {
      return true;
    }
    return false;
  }
  bool operator>(Matrix m) {
    return !((*this) < m) && (*this) != m;
  }
  bool operator<=(Matrix m) {
    return (*this) < m || (*this) == m;
  }
  bool operator>=(Matrix m) {
    return (*this) > m || (*this) == m;
  }
  template<class S>
  Matrix<S> operator=(S x) {
    for (int i = 0; i < this->width; i++) {
      for (int j = 0; j < this->height; j++) {
        if (x == 1 && i != j)
          continue;
        (*this)[i][j] = x;
      }
    }
    return (*this);
  }
  Matrix operator=(Matrix m) {
    this->arr = m.arr;
    this->width = m.width;
    this->height = m.height;
    return *this;
  }
  template<class S>
  Matrix operator+(S x) {
    Matrix result(this->height, this->width);
    for (int i = 0; i < this->height; i++) {
      for (int j = 0; j < this->width; j++) {
        result[i][j] = (*this)[i][j] + x;
      }
    }
    return result;
  }
  Matrix operator+(Matrix m) {
    Matrix result(this->height, this->width);
    for (int i = 0; i < this->height; i++) {
      for (int j = 0; j < this->width; j++) {
        result[i][j] = (*this)[i][j] + m[i][j];
      }
    }
    return result;
  }
  template<class S>
  Matrix operator*(S x) {
    Matrix result(this->width, this->height);
    for (int i = 0; i < this->width; i++) {
      for (int j = 0; j < this->height; j++) {
        result[i][j] = (*this)[i][j] * x;
      }
    }
    return result;
  }
  Matrix operator*(Matrix m) {
    if (this->width == m.height) {
      Matrix result(this->height, m.width);
      for (int i = 0; i < this->height; i++) {
        for (int j = 0; j < m.width; j++) {
          for (int k = 0; k < this->width; k++) {
            result[i][j] += (*this)[i][k] * m[k][j];
          }
        }
      }
      return result;
    }
    throw "Error: The height of first matrix should be equal to the width of the second matrix in multiplication.\n";
  }
  template<class S>
  Matrix operator/(S x) {
    Matrix result(this->width, this->height);
    for (int i = 0; i < this->width; i++) {
      for (int j = 0; j < this->height; j++) {
        result[i][j] = (*this)[i][j] / x;
      }
    }
    return result;
  }
  Matrix operator/(Matrix m) {
    if (this->width == m.height && m.width == m.height) {
      Matrix result(this->height, m.width);
      result = (*this) * (m ^ -1);
      return result;
    }
    throw "Error: The height of first matrix should be equal to the width of the second matrix and second matrix should be square in derivation.\n";
  }
  Matrix operator^(int x) {
    if (this->height == this->width) {
      int n = this->width;
      Matrix result(n, n);

      if (x < 0) {
        Matrix inverse(n, n);
        inverse = cofactor(*this) * (1 / det(*this));
        result = inverse ^ (x * -1);
      } else if (x == 0) {
        result = (T) 1;
      } else {
        result = (*this);
        for (int i = 0; i < x - 1; i++) {
          result = result * (*this);
        }
      }
      return result;
    } else {
      throw "A matrix can raised to a certain power only if it has the same height and width length.\n";
    }
  }

  void set(int x, int y, T value) {
    arr[y + width * x] = value;
  }

  void cset(int x, int y, char value) {
    arr[y + width * x] += value;
  }

  T &get(int x, int y) {
    return arr[y + width * x];
  }
};
template<class T>
double det(Matrix<T> mat) {
  int n = mat.width;
  double d; //det m
  if (n == 1) {
    return mat[0][0];
  }
  if (n == 2) {
    return ((mat[0][0] * mat[1][1]) - (mat[1][0] * mat[0][1]));
  } else {
    Matrix<T> submat(n - 1, n - 1);
    for (int c = 0; c < n; c++) {
      int subi = 0; //submatrix's i value
      for (int i = 1; i < n; i++) {
        int subj = 0;
        for (int j = 0; j < n; j++) {
          if (j == c)
            continue;
          submat[subi][subj] = mat[i][j];
          subj++;
        }
        subi++;
      }
      d += (pow(-1, c) * mat[0][c] * det(submat));
    }
  }
  return d;
}
// to calculate cofactor should caluculate minor then cofactor: https://en.wikipedia.org/wiki/Minor_(linear_algebra)
template<class T>
double Minor(Matrix<T> mat, int x, int y) {
  int n = mat.width;
  double d; //minor m
  Matrix<T> submat(n - 1, n - 1);

  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      if (i == x or j == y)
        continue;
      submat[i > x ? i - 1 : i][j > y ? j - 1 : j] = mat[i][j];
    }
  }
  return det(submat);
}
template<class T>
Matrix<T> Minor(Matrix<T> mat) {
  int n = mat.width;
  Matrix<T> submat(n, n);
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      submat[i][j] = Minor(mat, i, j);
    }
  }
  return submat;
}

template<class T>
double cofactor(Matrix<T> mat, int x, int y) {
  return pow(-1, x + y) * (mat.width == 2 && x + y == 1 ? mat[x][y] : Minor(mat, x, y));
}
template<class T>
Matrix<T> cofactor(Matrix<T> mat) {
  int n = mat.width;
  Matrix<T> submat(n, n);
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      submat[i][j] = cofactor(mat, i, j);
    }
  }
  return submat;
}

#endif //BULLDOG_MODULES_CORE_INCLUDE_BULLDOG_MATRIX_H_
