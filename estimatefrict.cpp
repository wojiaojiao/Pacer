#include <Moby/MatrixN.h>
#include <Moby/VectorN.h>
#include <Moby/LinAlg.h>
#include <Moby/Optimization.h>

const double NEAR_ZERO = sqrt(std::numeric_limits<double>::epsilon()); //2e-12;

typedef Moby::MatrixN Mat;
typedef Moby::VectorN Vec;

void outlog(const Mat& M, std::string name);
void outlog(const Vec& z, std::string name);
void outlog2(const Vec& M, std::string name);
void outlog2(const Mat& z, std::string name);

struct ContactData;

extern Vec cf_moby;

 void setblock(Mat& M, int I, int J, int P, int Q, const Mat& m){
     for(int i=0,ii=I;i<P;i++,ii++)
         for(int j=0,jj=J;j<Q;j++,jj++)
             M(ii,jj) = m(i,j);
 }

static bool solve_qp( Mat& Q,  Vec& c,  Mat& A,  Vec& b, Vec& x)
{
  const int n = Q.rows();
  const int m = A.rows();

  x.resize(n);
  // setup the LCP matrix
  // MMM = |  Q -Q -A' |
  //       | -Q  Q  A' |
  //       |  A -A  0  |
  Mat MMM;
  Vec zzz,qqq;
  MMM.set_zero(Q.rows()*2 + A.rows(), Q.rows()*2 + A.rows());
  setblock(MMM,0,0,n,n,Q);
  setblock(MMM,n,n,n,n,Q);
  Q.negate();
  setblock(MMM,0,n,n,n,Q);
  setblock(MMM,n,0,n,n,Q);

  // setup linear inequality constraints in LCP matrix
  Mat AT(A.columns(),A.rows());
  Mat::transpose(A,AT);
  setblock(MMM,n,n*2,n,m,AT);
  AT.negate();
  setblock(MMM,0,n*2,n,m,AT);

  setblock(MMM,n*2,0,m,n,A);
  A.negate();
  setblock(MMM,n*2,n,m,n,A);

  // setup LCP vector
  // qqq = [ c -c  b  0 ]'
  qqq.resize(MMM.rows());
  qqq.set_sub_vec(0,c);
  c.negate();
  qqq.set_sub_vec(n,c);
  b.negate();
  qqq.set_sub_vec(2*n,b);

  // solve the LCP
  zzz.set_zero(qqq.size());

  bool SOLVE_FLAG = true;
  if(!Moby::Optimization::lcp_lemke_regularized(MMM,qqq,zzz))
      SOLVE_FLAG = false;

  // extract x
  for(int i=0;i<n;i++)
      x[i] = zzz[i] - zzz[n+i];
  return SOLVE_FLAG;
}

bool first = true;

//#define USE_D

/// Friction Estimation
/// Calculates Coulomb friction at end-effectors
double friction_estimation(const Vec& v, const Vec& f, double dt,
                         const Mat& N,const Mat& D, const Mat& M, bool post_event, Mat& MU, Vec& cf)
{
    static int ITER = 0;
    static Vec v_, f_;  // previous values
    double norm_error = -1;
    int nc = N.columns();

    if(post_event){
        if(nc>0){
            ITER++;
            std::cout << "************** Friction Estimation **************" << std::endl;
            std::cout << "ITER: " << ITER << std::endl;
            std::cout << "dt = " << dt << std::endl;
            outlog2(N,"N");
            outlog2(D,"D");
            outlog2(M,"M");
            outlog2(v,"post-event-vel");
            outlog2(v_,"pre-event-vel");
            outlog2(f_,"f_external");

            int ngc = f_.rows();
            int nq = ngc - 6, nk = D.columns()/nc;
            int nvars = nc+nk*nc;
            cf.resize(nc + nc*(nk/2));

            Vec dv =  v - v_;
            outlog2(dv,"dv");

            // effective force (t-1 ==> t)
            Vec jstar(M.rows());
            M.mult(dv,jstar);
            outlog2(jstar,"j_observed");

            // acting force (last timestep)
            f_ *= dt;

            outlog2(f_,"j_expected");

            // error in impulse (obs - exp) = err
            jstar -= f_;
            outlog2(jstar,"j_error");

            /// //////////////////////////////
            /// STAGE I
    #ifdef USE_D
            int n = N.columns()+D.columns();

            Mat R(ngc,n);
            setblock(R,0,0,N.rows(),N.columns(),N);
            setblock(R,0,N.columns(),D.rows(),D.columns(),D);

            /////////// OBJECTIVE ////////////
            // M
            Mat Q(R.columns(),R.columns());
            R.transpose_mult(R,Q);
            // c
            Vec c(R.columns());
            R.transpose_mult(jstar,c);
            // Inequality constraint
            c.negate();

            Vec z(n);
            z.set_zero();
            if (!Moby::Optimization::lcp_lemke_regularized(Q,c,z)){
                std::cout << "friction estimation failed" << std::endl;
            } else {

                outlog2(z,"z");

                Vec err(ngc);
                R.mult(z,err);
                outlog2(err,"gf");
                err -= jstar;

                outlog2(err,"err");
                std::cout << "norm err: " << err.norm() << std::endl;
    #else // use ST
            Mat ST;
            ST.resize(D.rows(),D.columns()/2);
            ST.set_zero();
            // remove negations from D to create ST
            // of the form [S T]

            for(int i=0;i<nc;i++){
                for(int j=0;j<ngc;j++){
                    ST(j,i) = D(j,i*nk);
                    ST(j,nc+i) = D(j,i*nk+1);
                }
            }
           int n = N.columns()+ST.columns();

           Mat R(ngc,n);
           setblock(R,0,0,N.rows(),N.columns(),N);
           setblock(R,0,N.columns(),ST.rows(),ST.columns(),ST);

           /////////// OBJECTIVE ////////////
           // Q = R'R
           Mat Q(R.columns(),R.columns());
           R.transpose_mult(R,Q);
           // c = R' M (v_{t} - v_{t-1})
           Vec c(R.columns());
           R.transpose_mult(jstar,c);
           c.negate();

           ////////// CONSTRAINT ///////////

           Mat A(nc,n);
           A.set_zero();
           Vec b(nc);
           b.set_zero();
           for(int i=0;i<nc;i++)
               A(i,i) = 1;

           Vec z(n);
           z.set_zero(n);

           if (!solve_qp(Q,c,A,b,z)){
               std::cout << "friction estimation failed" << std::endl;
           } else {
               outlog2(z,"z");

               Vec err(ngc);
               R.mult(z,err);
               outlog2(err,"generalized force from cfs = [R*z]");
               err -= jstar;
               norm_error =  err.norm();

               outlog2(err,"err = [R*z - j_error]");
               std::cout << "norm err: " << err.norm() << std::endl;

               //// TEST AGAINST MOBY FORCE ////
               R.mult(cf_moby,err);
               outlog2(err,"MOBY generalized force from cfs = [R*z]");

               const Vec &err_ptr = err;
               Vec dvM(err_ptr);
               Mat iM = M;
               Moby::LinAlg::factor_chol(iM);
               Moby::LinAlg::solve_chol_fast(iM,dvM);
               outlog2(dvM,"MOBY dv = [R*z - j_error]/M");



               err -= jstar;
               norm_error =  err.norm();

               outlog2(err,"MOBY err = [R*z - j_error]");
               std::cout << "MOBY  norm err: " << err.norm() << std::endl;
    #endif


               /// //////////////////////////////
               /// STAGE II

                // Q = R'R = RTR
                int m = 0;
                Mat U,V;
                Vec S;
                Moby::LinAlg::svd(Q,U,S,V);

                Mat P;
                // SVD decomp to retrieve nullspace of R'R
                if(S.rows() != 0)
                {
                    // Calculate the tolerance for ruling that a singular value is supposed to be zero
                    double ZERO_TOL = std::numeric_limits<double>::epsilon() * Q.rows() * S[0];
                    // Count Zero singular values
                    for(int i = S.rows()-1;i>=0;i--,m++)
                        if(S[i] > ZERO_TOL)
                            break;

                    // get the nullspace
                    P.resize(nvars,m);
                    P = V.get_sub_mat(0,V.rows(),V.columns() - m,V.columns());
                }

                std::cout << "m: " << m << std::endl;

                if(m != 0)
                {
    //                outlog2(P,"P");

                    Vec cN = z.get_sub_vec(0,nc);

                    /// Objective
                    Mat Q2(m,m);
                    Vec c2(m), w(m);

                    /// min l2-norm(cN)
                    Mat P_nc = P.get_sub_mat(0,nc,0,m);
    //                outlog2(P_nc,"P_nc");
    //                P_nc.transpose_mult(P_nc,Q2);
                    // c = P'cN
    //                P_nc.transpose_mult(cN,c2);

                    /// min l2-norm(cf)
                    P.transpose_mult(P,Q2);
                    // c = P'z
                    P.transpose_mult(z,c2);

                    /// min l2-norm(cST)
    //                Mat P_tc = P.get_sub_mat(nc,P.rows(),0,m);
    //                P_tc.transpose_mult(P_tc,Q2);
                    // c = P'z
    //                P_tc.transpose_mult(z.get_sub_vec(nc,z.rows()),c2);
    #ifdef USE_D
                    Mat A(nvars+1,m);
                    Vec  b(nvars+1);
                    //      A     w        b
                    //  | R'jP || w | <= |+0 |
                    //  |   P  || : | >= |-z |


                    //      A        w        b
                    //  |-R'jP |   | w |    |+0 |
                    //  |   P  | * | . | >= |-z |
                    //  |   .  |   | . |    | . |
                    //  |   .  |            | . |
                    //  |   .  |            | . |
                    //  |   .  |            | . |

                    A.set_zero();

                    Vec cP(m);
                    //-R'jP
                    P.transpose_mult(c,cP);
                    A.set_row(0,cP);

                    b[0] = 0;//c.dot(z);
                    //  |   P  || : | >= |-z |
                    Mat nP = -P;
                    A.set_sub_mat(1,0,nP);

                    // b = z
                    Vec cNST = -z;
                    outlog2(cNST,"cNST");
                    outlog2(z,"z");

                    b.set_sub_vec(1,z);
    #else // USE_ST
                    Mat A(nc+1,m);
                    Vec  b(nc+1);
                    A.set_zero();
                    b.set_zero();
                    //      A           w         b
                    //  |    R'jP    || w | <= | +0 |
                    //  |  P[1:nc,:] || : | >= |-cN |
                    /// -------------------------------
                    //  |   -R'jP   |   | w |    | +0 |
                    //  | P[1:nc,:] | * | . | >= |-cN |
                    //                  | . |

                    //  |   -R'jP   |   | w |    | +0 |
                    Vec cP(m);
                    P.transpose_mult(c,cP);
                    A.set_row(0,cP);
                    b[0] = 0;

                    //  | P[1:nc,:] | * | w | >= |-cN |
                    A.set_sub_mat(1,0,P_nc);
                    // b = cN
                    cN.negate();
                    b.set_sub_vec(1,cN);
    #endif
                    if(!solve_qp(Q2,c2,A,b,w)) {
                        std::cout << "friction estimation 2 failed" << std::endl;
                    } else {
                        outlog2(w,"w");

                        Vec z2(nvars);
                        P.mult(w,z2);

                        outlog2(z2," z2");
                        z += z2;
                        outlog2(z,"z+z2");

                        err.set_zero(ngc);
                        R.mult(z,err);
                        outlog2(err,"generalized force from cfs = [R*(z+z2)]");
                        err -= jstar;

                        outlog2(err,"err = [R*(z+z2) - j_error]");
                        norm_error =  err.norm();

                        std::cout << "norm err2: " << err.norm() << std::endl;
                    }
                }
            }
    #ifdef USE_D
            for(int i = 0;i < nc;i++)
            {
                cf[i] = z[i];
                cf[nc+i] = z[nc+nk*i] - z[nc+nk*i+nk/2];
                cf[nc*2+i] = z[nc+nk*i+1] - z[nc+nk*i+1+nk/2];
            }
    #else
            cf = z;
    #endif
            for(int i = 0;i < nc;i++){
                if(cf[i] > 0)
                    MU(i,0) = sqrt(cf[nc+i]*cf[nc+i] + cf[nc*2+i]*cf[nc*2+i]) / cf[i];
                else
                    MU(i,0) = sqrt(-1);

                std::cout << "cf Estimate = [" << cf[i+nc] << " " << cf[i+nc*2] << " " << cf[i] << "]" << std::endl;
                std::cout << "MU_Estimate : " << MU(i,0) << std::endl;
            }
        }
        v_ = v;
    } else {
        f_ = f;

    }

    first = false;

    return norm_error;
}

