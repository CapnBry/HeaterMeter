/*
 * Project:  C to Lua wrap of LevenbergMarquardtLeastSquaresFitting
 *
 * File:     lualmfit.c
 *
 * Contents: Public interface to the Levenberg-Marquardt core implementation.
 *
 * Author:   Lucas Lorensi 2009-2 
 * 
 * Homepage: http://luaforge.net/projects/lualmfit/
 *
 * Licence:  Public domain.
 */

#include <stdlib.h>
#include <math.h>
#include "lmmin.h"

#include "lua.h"
#include "lauxlib.h"


/*
  Lua data type
*/
typedef double (*lua_lm_user_func) (double user_t_point, double *par, int n_p, lua_State* L);

typedef struct {
    double *user_t;
    double *user_y;
    int n_p;
	  lua_State* L;
    lua_lm_user_func user_func;
} lua_lm_data_type;


/*
  Perform error evaluation.(TODO: make it a Lua user-define function)
*/
static void lua_lm_evaluate_default(double *par, int m_dat, double *fvec,
			 void *data, int *info)
{
    int i;
    lua_lm_data_type *mydata;
    mydata = (lua_lm_data_type *) data;

    for (i = 0; i < m_dat; i++)
      fvec[i] = mydata->user_y[i] - mydata->user_func(mydata->user_t[i], par, mydata->n_p, mydata->L);

    *info = *info;		/* to prevent a 'unused variable' warning */
    /* if <parameters drifted away> { *info = -1; } */
}


/*
  Calls lua user-defined printout routine.(TODO: make it a Lua user-define function)
*/
static void lua_lm_print_default(int n_par, double *par, int m_dat, double *fvec,
		      void *data, int iflag, int iter, int nfev)
/*
 *       data  : for soft control of printout behaviour, add control
 *                 variables to the data struct
 *       iflag : 0 (init) 1 (outer loop) 2(inner loop) -1(terminated)
 *       iter  : outer loop counter
 *       nfev  : number of calls to *evaluate
 */
{
  /*
    double f, y, t;
    int i;
    lm_data_type *mydata;
    mydata = (lm_data_type *) data;

    if (iflag == 2) {
	printf("trying step in gradient direction\n");
    } else if (iflag == 1) {
	printf("determining gradient (iteration %d)\n", iter);
    } else if (iflag == 0) {
	printf("starting minimization\n");
    } else if (iflag == -1) {
	printf("terminated after %d evaluations\n", nfev);
    }

    printf("  par: ");
    for (i = 0; i < n_par; ++i)
	printf(" %12g", par[i]);
    printf(" => norm: %12g\n", lm_enorm(m_dat, fvec));

    if (iflag == -1) {
	printf("  fitting data as follows:\n");
	for (i = 0; i < m_dat; ++i) {
	    t = (mydata->user_t)[i];
	    y = (mydata->user_y)[i];
	    f = mydata->user_func(t, par);
	    printf("    t[%2d]=%12g y=%12g fit=%12g residue=%12g\n",
		   i, t, y, f, y - f);
	}
    }
  */
}


/*
  Fill a C vector from a Lua table
*/
static void getdata( lua_State* L, int index, double *vector, int count ) 
{
  int i;
  for( i=1; i <= count; ++i )
  {
    lua_pushinteger(L,i);
    lua_gettable(L,index);
    vector[i-1] = luaL_checknumber(L, -1);
    lua_pop(L, 1);
  }
}


/*
  Fill a Lua table(top of the stack) from a C vector
*/
static void setdata( lua_State* L, double* vector, int count )
{
  int i;
  lua_newtable(L);
  for( i=0; i < count; ++i )
  {
    lua_pushinteger( L, i+1 );
    lua_pushnumber( L, vector[i] );
    lua_settable(L, -3 );
  }
}


/*
  Calls your Lua user-define fit function.
*/
static double your_fit_function(double t, double *p, int n_p, lua_State* L)
{
  lua_pushvalue(L,-1);
  lua_pushnumber(L,t);
  setdata(L,p,n_p);
  lua_call(L,2,1);
  double value = luaL_checknumber(L,-1);
  lua_pop(L,1);
  return value;
}

static double steinhart_fit_function(double t, double *p, int n_p, lua_State* L)
{
  double lx = log(t);
  return 1.0 / ((p[2] * lx * lx + p[1]) * lx + p[0]) - 273.15;
}

static double linear_fit_function(double t, double *p, int n_p, lua_State* L)
{
  double retVal = t * p[0];
  if (n_p > 1)
    retVal += p[1];
  return retVal;
}

/*
  Run lmfit algorithm  
*/
static int lua_lm_do_fit( lua_State* L, lua_lm_user_func user_func)
{
  int m_dat_t = (int)lua_objlen(L,1);
  int m_dat_y = (int)lua_objlen(L,2);
  int n_p = (int)lua_objlen(L,3);

  if( m_dat_t <= 0 ||
      m_dat_y <= 0 ||
      n_p <= 0 ||
      m_dat_t != m_dat_y )
    luaL_error( L, "invalid vector size" );

  int m_dat = m_dat_t;

  double *t = (double *)malloc(m_dat * sizeof(double));
  double *y = (double *)malloc(m_dat * sizeof(double));
  double *p = (double *)malloc(n_p * sizeof(double));

  getdata( L, 1, t, m_dat );
  getdata( L, 2, y, m_dat );
  getdata( L, 3, p, n_p );

  // auxiliary settings:

  lm_control_type control;
  lua_lm_data_type data;

  lm_initialize_control(&control);

  data.user_func = user_func;
  data.user_t = t;
  data.user_y = y;
  data.L = L;
  data.n_p = n_p;

  // perform the fit:

  lm_minimize(m_dat, n_p, p, lua_lm_evaluate_default, lua_lm_print_default, &data, &control);

  setdata( L, p, n_p );
  lua_pushinteger(L,control.info);
  lua_pushinteger(L,control.nfev);

  free(t);
  free(y);
  free(p);

  return 3;
}


/*
  lmfit.minimize(x[], y[], estimate[], fit_function)
*/
static int minimize(lua_State* L)
{
  return lua_lm_do_fit(L, &your_fit_function);
}

/*
  lmfit.steinhart(resists[], tempsC[])
  Calculation of the Steinhart-Hart coefficients.
  resists[] - table of resistance in ohms
  tempsC[] - table of temperatures in Celcius
  both tables must be the same length
*/
static int steinhart(lua_State* L)
{
  // Create a table with estimates for the steinhart coefficients
  lua_newtable(L);
  lua_pushinteger(L, 1); lua_pushnumber(L, 1.0e-4); lua_rawset(L, -3);
  lua_pushinteger(L, 2); lua_pushnumber(L, 1.0e-4); lua_rawset(L, -3);
  lua_pushinteger(L, 3); lua_pushnumber(L, 1.0e-7); lua_rawset(L, -3);
  return lua_lm_do_fit(L, &steinhart_fit_function);
}

/*
  lmfit.linear(x[], y[], [m_est, b_est])
  Simple linear regression fit
  x[] - table of x values
  y[] - table of y values
  m_est - slope estimate
  b_est - intercept estimate
*/
static int linear(lua_State* L)
{
  return lua_lm_do_fit(L, &linear_fit_function);
}

static int shortmsg(lua_State* L)
{
  int msg = luaL_checkint(L, 1);
  if (msg >= 11) // countof(lm_shortmsg)
    return 0;

  lua_pushstring(L, lm_shortmsg[msg]);  
  return 1; 
}

static int message(lua_State* L)
{
  int msg = luaL_checkint(L, 1);
  if (msg >= 11) // countof(lm_infmsg)
    return 0;

  lua_pushstring(L, lm_infmsg[msg]);  
  return 1; 
}

/*
  Functions register array.
*/
static struct luaL_Reg regs[] = {
  {"message", message},
  {"minimize", minimize},
  {"shortmsg", shortmsg},
  {"steinhart", steinhart},
  {"linear", linear},
  {NULL, NULL}
};


/*
  Open function.
*/
int luaopen_lmfit(lua_State* L)
{
  luaL_register(L, "lmfit", regs );
  //luaL_setfuncs(L, regs, 0);
  return 1;
}

