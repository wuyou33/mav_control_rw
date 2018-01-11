clc;
clear all;
close all;

Ts = 0.1; %prediction sampling time %0.05
EXPORT = 1;

DifferentialState p(3,1) v(3,1) mrp(3,1) w(3,1);
Control f1 f2 f3 f4 f5 f6;

OnlineData mass;
OnlineData inertia(3); % vector Ixx, Iyy, Izz 1x3 vec?
OnlineData drag_coefficient; % c=0.0001;
OnlineData armlength;
OnlineData angVel_ref;
OnlineData yaw_ref; %pi/4  //should reference and arm length be online data?
OnlineData external_forces(3);
%OnlineData quat_norm_gain;

n_XD = length(diffStates);
n_U = length(controls);

g = 9.8066; %g = 9.81;
m=mass; %1.9;
J=diag(inertia); %diag([0.09,0.09,0.17]); %Inertia
inv_J=diag([1/inertia(1), 1/inertia(2), 1/inertia(3)]); %inv(J);
l= armlength; %length
c=drag_coefficient; %0.0001; %drag coefficient

% %references
% yaw_ref=pi/4;
q_ID=[cos(yaw_ref/2); 0; 0; sin(yaw_ref/2)];
%R_ID=[cos(yaw_ref) -sin(yaw_ref) 0; sin(yaw_ref) cos(yaw_ref) 0; 0 0 1];
u_ss=(m*g/n_U)*ones(n_U,1); %from outside 
u_ss_total= m*g; %mg
%w_B_ID= [angVel_ref;0;0];  % should be online data since desired angular vel??
%% Differential Equation

S=[0 -mrp(3) mrp(2);mrp(3) 0 -mrp(1); -mrp(2) mrp(1) 0];

q0= ( 1-(mrp(1)^2 + mrp(2)^2 + mrp(3)^2) )/(1 + mrp(1)^2 + mrp(2)^2 + mrp(3)^2 );
q13= (2/(1 + mrp(1)^2 + mrp(2)^2 + mrp(3)^2 )) * mrp;

A=[0.0435778713738291,-0.0871557427476582,0.0435778713738291,0.0435778713738291,-0.0871557427476582,0.0435778713738291;-0.0754790873051733,0,0.0754790873051733,-0.0754790873051733,0,0.0754790873051733;0.996194698091746,0.996194698091746,0.996194698091746,0.996194698091746,0.996194698091746,0.996194698091746;0.143469078891783,0.286938157783566,0.143469078891783,-0.143469078891783,-0.286938157783566,-0.143469078891783;-0.248495733955676,0,0.248495733955676,0.248495733955676,0,-0.248495733955676;-0.0419218334972761,0.0419218334972761,-0.0419218334972761,0.0419218334972761,-0.0419218334972761,0.0419218334972761];
F_M= A*[f1;f2;f3;f4;f5;f6];

aux=[0;rotate_quat([q0;q13],[0;0;0;F_M(3)])]-[0; 0; 0; u_ss_total];  
vdot=aux(2:4);

e_q=quat_mult([q_ID(1); -q_ID(2:4)],[q0; q13]);
e_mrp= e_q(2:4)/(1+e_q(1));



mrp_dot= 0.25*((1-mrp'*mrp)*eye(3) + 2*S + 2*mrp*mrp')*w;

M = F_M(4:6); %moments % include sign of ftotal in yawing moment



f = dot([p; v; mrp; w]) == ...
    [v;...
    (1/m)* vdot + external_forces; ... %+ external_forces
    mrp_dot;... 
    inv_J*(M-cross( w, J*w )) ;...
    ];

h = [p;...
    v;...
    e_mrp;...
    w;...
    [f1 f2 f3 f4 f5 f6]' - u_ss]; %- u_ss

hN = [p;...
    v];

%% MPCexport
acadoSet('problemname', 'mav_modified_rodrigues_nmpc'); %'barza_mpc'

N = 15; %40
ocp = acado.OCP( 0.0, N*Ts, N );

W_mat = eye(length(h));
WN_mat = eye(length(hN));
W = acado.BMatrix(W_mat);
WN = acado.BMatrix(WN_mat);

ocp.minimizeLSQ( W, h );
ocp.minimizeLSQEndTerm( WN, hN );
ocp.subjectTo(-8.0 <= [f1; f2; f3; f4; f5; f6] <= 8.0);
%ocp.subjectTo(-5 <= e_w <= 5);
%ocp.subjectTo(-2.5 <= e_mrp <= 2.5);
%ocp.subjectTo(cos(65*pi/180) <= R_DB(3,3)  );
ocp.setModel(f);


mpc = acado.OCPexport( ocp );
mpc.set( 'HESSIAN_APPROXIMATION',       'GAUSS_NEWTON'      );
mpc.set( 'DISCRETIZATION_TYPE',         'MULTIPLE_SHOOTING' );
mpc.set( 'SPARSE_QP_SOLUTION',        'FULL_CONDENSING_N2'  ); %FULL_CONDENsinG_N2
mpc.set( 'INTEGRATOR_TYPE',             'INT_IRK_GL4'       );
mpc.set( 'NUM_INTEGRATOR_STEPS',         N                  );
mpc.set( 'QP_SOLVER',                   'QP_QPOASES'    	);
mpc.set( 'HOTSTART_QP',                 'NO'             	);
mpc.set( 'LEVENBERG_MARQUARDT',          1e-10				);


mpc.set( 'LINEAR_ALGEBRA_SOLVER',        'GAUSS_LU'         ); %Do we need this?
mpc.set( 'IMPLICIT_INTEGRATOR_NUM_ITS',  5                  );
mpc.set( 'CG_USE_OPENMP',                'YES'              );
mpc.set( 'CG_HARDCODE_CONSTRAINT_VALUES', 'NO'              );
mpc.set( 'CG_USE_VARIABLE_WEIGHTING_MATRIX', 'NO'           );

% if EXPORT
%     mpc.exportCode( 'export_MPC' );
%     copyfile('../../../../../../external_packages/qpoases', 'export_MPC/qpoases', 'f')
%     
%     cd export_MPC
%     make_acado_solver('../acado_MPCstep')
%     cd ..
% end

if EXPORT
    mpc.exportCode('.');
end


function [rotated_quat]=rotate_quat(q,v) 
% q and v are 4x1 quats
anss= quat_mult(quat_mult(q,v), [q(1); -q(2:4)]);
rotated_quat=anss(2:4); %to covert to 3x1 vec
end

function [mult_quat]=quat_mult(q,p)
%q and p are 4x1 quats
mult_quat=[ p(1)*q(1) - p(2)*q(2) - p(3)*q(3) - p(4)*q(4), p(1)*q(2) + p(2)*q(1) - p(3)*q(4) + p(4)*q(3), p(1)*q(3) + p(3)*q(1) + p(2)*q(4) - p(4)*q(2), p(1)*q(4) - p(2)*q(3) + p(3)*q(2) + p(4)*q(1)]';
%returns 4x1 quat 
end

