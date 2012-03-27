#include <iostream>
#include <limits>
#include "FVLib.h"

//	BEGIN CONSTANTS

/**
 * OpenMP [f]actor
 * Used with the number of processors to calculate the number of threads
 * pc -> processor count
 * tc -> thread count
 * => tc = pc * f
 * This allows the existence of more threads than the hardware is capable, therefore making possible for some threads to step in while others wait on resources (based on GPU approach).
 */
#define	OMP_FCT_ALL	1

//	END CONSTANTS

//	BEGIN TYPES

/*
	Parameters: holds the data from the parameter file
*/
typedef
struct _parameters
{
	struct
	{
		string mesh;
		string velocity;
		struct
		{
			string initial;
			string output;
		} polution;
	} filenames;
	struct
	{
		double final;
	} time;
	struct
	{
		int jump;
	} iterations;
	struct
	{
		double threshold;
	} computation;
}
Parameters;

//	END TYPES

//	BEGIN GLOBAL VARIABLES

int tc;										//	thread count
double *max_vel_v;							//	thread maximum computed velocity vector

//	END GLOBAL VARIABLES

//	BEGIN FUNCTIONS

/*
	Computes the resulting flux in every edge
*/
double compute_flux(
	FVMesh2D& mesh,
	FVVect<double>& polution,
	FVVect<FVPoint2D<double> >& velocity,
	FVVect<double>& flux,
	double dc)								//	Dirichlet condition
{
	double dt;
	double p_left;							//	polution in the left face
	double p_right;							//	polution in the right face
	double v_max;							//	maximum computed velocity
	int i_left;								//	index of the left face
	int i_right;							//	index of the right face
	int t;									//	current thread number
	unsigned e;								//	edge iteration variable
	unsigned es;							//	total number of edges
	FVPoint2D<double> v_left;				//	velocity in the left face
	FVPoint2D<double> v_right;				//	velocity in the right face
	double v;								//	resulting velocity
	FVEdge2D *edge;							//	current edge

	es = mesh.getNbEdge();
	#pragma omp parallel	\
		default(shared)	\
		num_threads(tc)	\
		private(t,e,edge,i_left,v_left,p_left,i_right,v_right,p_right,v)
	{
		t = omp_get_thread_num();

		max_vel_v[t] = numeric_limits<double>::min();

		#pragma omp for
		for (e = 0; e < es; ++e)
		{
			edge = mesh.getEdge(e);
			i_left = edge->leftCell->label - 1;
			v_left = velocity[ i_left ];
			p_left = polution[ i_left ];
			if ( edge->rightCell ) 
			{
				i_right = edge->rightCell->label - 1;
				v_right = velocity[ i_right ];
				p_right = polution[ i_right ];
			}
			else
			{
				v_right = v_left;
				p_right = dc;
			} 
			v = ( v_left + v_right ) * 0.5 * edge->normal; 
			max_vel_v[t] = ( v > max_vel_v[t] ) ? v : max_vel_v[t];
			if ( v < 0 )
				flux[ edge->label - 1 ] = v * p_right;
			else
				flux[ edge->label - 1 ] = v * p_left;
		}
	}

	v_max = numeric_limits<double>::min();
	for (t = 0; t < tc; ++t)
		v_max = ( max_vel_v[t] > v_max ) ? max_vel_v[t] : v_max;
	
	dt = 1.0 / abs( v_max );

	return dt;
}

/*
	Updates the polution values based on the flux through every edge.
*/
void update(
	FVMesh2D& mesh,
	FVVect<double>& polution,
	FVVect<double>& flux,
	double dt)
{
	FVEdge2D *edge;

	int es = mesh.getNbEdge();
	for (int e = 0; e < es; ++e)
	{
		edge = mesh.getEdge(e);
		polution[ edge->leftCell->label - 1 ] -=
			dt * flux[ edge->label - 1 ] * edge->length / edge->leftCell->area;
		if ( edge->rightCell )
			polution[ edge->rightCell->label - 1 ] +=
				dt * flux[ edge->label - 1 ] * edge->length / edge->rightCell->area;
	}
}    

/*
	Reads the parameters file.
*/
Parameters read_parameters (
	string parameter_filename)
{
	Parameters data;
	Parameter para( parameter_filename.c_str() );

	data.filenames.mesh = para.getString("MeshName");
	data.filenames.velocity = para.getString("VelocityFile");
	data.filenames.polution.initial = para.getString("PoluInitFile");
	data.filenames.polution.output = para.getString("PoluFile");
	data.time.final = para.getDouble("FinalTime");
	data.iterations.jump = para.getInteger("NbJump");
	data.computation.threshold = para.getDouble("DirichletCondition");

	return data;
}

/*
	Computes the mesh parameter (whatever that is)
*/
double compute_mesh_parameter (
	FVMesh2D mesh)
{
	double h;
	double S;
	FVCell2D *cell;
	FVEdge2D *edge;

	h = 1.e20;
	for ( mesh.beginCell(); ( cell = mesh.nextCell() ) ; )
	{
		S = cell->area;
		for ( cell->beginEdge(); ( edge = cell->nextEdge() ) ; )
		{
			//edge = cell->getEdge(e);
			if ( h * edge->length > S )
				h = S / edge->length;
		}
	}
	return h;
}

/*
	Main loop: calculates the polution spread evolution in the time domain.
*/
void main_loop (
	double final_time,						//	time computation limit
//	unsigned jump_interval,					//	iterations output interval
	FVMesh2D mesh,							//	2D mesh to compute
	double mesh_parameter,					//	mesh parameter
	FVVect<double> polutions,				//	polution values vector
	FVVect<FVPoint2D<double> > velocities,	//	velocity vectors collection
	FVVect<double> fluxes,					//	flux values vector
	double dc,								//	Dirichlet condition
	string output_filename)					//	output file name
{
	double t;								//	time elapsed
	double dt;
	FVio polution_file( output_filename.c_str() ,FVWRITE);

	for ( t = 0 ; t < final_time ; t += dt )
	{
		dt = compute_flux( mesh , polutions , velocities , fluxes , dc ) * mesh_parameter;
		update( mesh , polutions , fluxes , dt );
	}
	polution_file.put( polutions , t , "polution" ); 
}

/*
	Função Madre
*/
int main(int argc, char** argv)
{  
	string name;
	double h;
	double t;
	FVMesh2D mesh;
	Parameters data;

	// read the parameter
	if (argc > 1)
		data = read_parameters( string(argv[1]).c_str() );
	else
		data = read_parameters( "param.xml" );

	// read the mesh
	mesh.read( data.filenames.mesh.c_str() );

	FVVect<double> polution( mesh.getNbCell() );
	FVVect<double> flux( mesh.getNbEdge() );
	FVVect<FVPoint2D<double> > velocity( mesh.getNbCell() );

	//	read velocity
	FVio velocity_file( data.filenames.velocity.c_str() , FVREAD );
	velocity_file.get( velocity , t , name );

	//	read polution
	FVio polu_ini_file( data.filenames.polution.initial.c_str() , FVREAD );
	polu_ini_file.get( polution , t , name );

	//	OpenMP init
	tc = omp_get_num_procs() * OMP_FCT_ALL;

	//	prepare velocities array
	max_vel_v = new double[ tc ];

	// compute the Mesh parameter
	h = compute_mesh_parameter( mesh );

	// the main loop
	main_loop(
		data.time.final,
//		data.iterations.jump,
		mesh,
		h,
		polution,
		velocity,
		flux,
		data.computation.threshold,
		data.filenames.polution.output)
	;

	delete max_vel_v;

	return 0;
}