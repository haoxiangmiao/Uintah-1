
# Solve the 1d heat equation:
# du/dt - k d^2/dx^2 (u) = 0, 
# u = temperature 
# k = thermal conductivity.

# form matrics
# apply bounday conditions
# solve system

1;

function [points, elems] = make_grid(bar_dim,spacing)
  count = 1;
  j = 0;
  while (j <= bar_dim)
    points(count++) = j;
    j += spacing;
  endwhile
  if (points(length(points)) != bar_dim)
    points(count) = bar_dim;
  endif

  e = 1;
  for (ro=1:length(points)-1)
    elems(ro,1) = e;
    elems(ro,2) = ++e;
  endfor
  
 endfunction

function [K,F] = initialize_K_F(num_pts)
  K(1:num_pts,1:num_pts) = 0;
  F(1:num_pts) = 0;
endfunction

function [K,F] = form_matrix(K,F,points,elems,materials,dt,theta,T)
  [nr,nc] = size(elems);

  for (elem_num = 1:nr)
    element(1) = elems(elem_num,1);
    element(2) = elems(elem_num,2);
    [KE,C] = element_linear(elem_num,element,points,materials);
    K = assemble(element,K,KE,C,dt,theta);
    F = source_term(element,F,KE,C,dt,theta,T);
  endfor

endfunction

function [KE,Ca] = element_linear(i,element,points,materials)
#  printf("Doing element linear\n");
  n1 = element(1);
  n2 = element(2);
  pts = [points(n1),points(n2)];
  [xi,weight] = gauss_quadrature(1);
  shape = shape_linear(xi,pts);

  kond = materials.kond(i);
  density = materials.density(i);
  specific_heat = materials.specific_heat(i);

  KE(1:2,1:2)=0;

  Ka = Kalpha(shape,weight,kond);
  Ca = Capacitance(shape,weight,density,specific_heat);

  KE = Ka;
   
endfunction

function K = assemble(element,K,KE,C,dt,theta)
  #printf("Doing assemble\n");

  for (i=1:2)
    ii = element(i);
    for (j=1:2)
      jj = element(j);
      K(ii,jj) += (C(i,j)/dt + theta*KE(i,j));
    endfor
  endfor

endfunction

function [xi,weight] = gauss_quadrature(order)
 # printf("Doing gauss_quadrature\n");
  if (order == 1)
    xi(1) = 0;
    weight(1) = 2;
  endif
  if (order == 2)
    xi(2,1) = -1/sqrt(3);
    xi(2,2) = -xi(2,1);
    weight(2,1) = 1;
    weight(2,2) = weight(2,1);
  endif
  if (order == 3)
    xi(3,1) = -sqrt(3/5);
    xi(3,2) = 0;
    xi(3,3) = -xi(3,1);
    weight(3,1) = 5/9;
    weight(3,2) = 8/9;
    weight(3,3) = weight(3,1);
  endif
endfunction

function shape = shape_linear(xi,pts)
  #printf("Doing shape_linear\n");

  shape.phi(1) = 1/2 * (1-xi);
  shape.phi(2) = 1/2 * (1+xi);

  shape.dphidxi(1) = -1/2;
  shape.dphidxi(2) = 1/2;
  
  shape.jac = compute_jacobian(pts);

  shape.dphidx(1) = shape.dphidxi(1) * 1/shape.jac;
  shape.dphidx(2) = shape.dphidxi(2) * 1/shape.jac;

endfunction

function jac = compute_jacobian(pts)
 # printf("Doing compute_jacobian\n");

  jac = 1/2 *(pts(2) - pts(1));
  
endfunction

function Ka = Kalpha(shape,weight,kond)
#  printf("Doing Kalpha\n");
  for(i=1:2)
    for(j=1:2)
      Ka(i,j) = weight*(shape.jac)*shape.dphidx(i)*kond*shape.dphidx(j);
    endfor
  endfor
    
endfunction

function Kbeta()
  printf("Doing Kbeta\n");
endfunction

function C = Capacitance(shape,weight,density,specific_heat)
   # printf("Doing Capacitance\n");
    for(i=1:2)
      for(j=1:2)
        C(i,j) = weight*shape.phi(i)*density*specific_heat*shape.phi(j);
      endfor
    endfor

endfunction

function F = source_term(element,F,KE,C,dt,theta,T)
 # printf("Doing source term\n");
  for (i=1:2)
    ii = element(i);
    for (j=1:2)
      jj = element(j);
      F(ii) += (C(i,j)/dt - (1 -theta)*KE(i,j))*T(jj);
    endfor
  endfor


endfunction

function bcs = generate_bcs(bc,p)
  #printf("Doing generate_bcs\n");
  bcs.n(1) = 1;
  bcs.v(1) = bc.left;
  bcs.n(2) = length(p);
  bcs.v(2) = bc.right;
  
endfunction

function [K,F] = apply_bcs(K,F,bcs)
   # printf("Doing apply_bcs\n");

    [nr,nc]=size(K);

    for (bc=1:2)
      node=bcs.n(bc);
      bcvalue = bcs.v(bc);
      for (j=1:nr)
        kvalue = K(j,node);
        F(j) -= kvalue*bcvalue;
        if (j != node)
          K(j,node) = 0;
          K(node,j) = 0;
        else
          K(j,node) = 1;
        endif
      endfor
      F(node) = bcvalue;
    endfor
    F = transpose(F);
endfunction

function a = solve_system(K,F)
#printf("Doing solve_system\n");
    a = K \ F;
endfunction

function mat = define_materials()
  num_mat = input("input number of materials ");
  for (i=1:num_mat)
    mat.le(i) = input("input left edge ");
    mat.re(i) = input("input right edge ");
    mat.kond(i) = input("input thermal conductivity ");
    mat.density(i) = input("input density ");
    mat.specific_heat(i) = input("input specific heat ");
  endfor

endfunction

function materials = create_materials_element(p,e,mat)

  [nr,nc] = size(e);
  for (elem = 1:nr)
    n1 = e(elem,1);
    n2 = e(elem,2);
    center = (p(n1) + p(n2))/2;
    for (i=1:length(mat.kond))
      if (mat.le(i) <= center && center <= mat.re(i))
        materials.kond(elem) = mat.kond(i);
        materials.density(elem) = mat.density(i);
        materials.specific_heat(elem) = mat.specific_heat(i);
      endif
    endfor

  endfor

endfunction

function T = set_intial_condition(initial_temp,num)

  T(1:num) = initial_temp;

endfunction


function main()
  
  bar = input("input size of bar ");
  spacing = input("input grid spacing ");
  bc.left = input("input left bc value ");
  bc.right = input("input right bc value ");

  initial_temp = input("input initial temperature ");

  theta = input("input theta (0 = explicit, .5 = midpoint, 1 = implicit  ");
  dt = 0;
  lump = true;
  if (theta < .5)
    dt = input("input dt ");
    lump = input("input lumping (true/false) ");
  endif
  end_time = input("input end time ");
  if (dt == 0)
    dt = input("input time step size ");
  endif
  
  mat = define_materials();

  if (theta < .5)
    kond_max = max(mat.kond);
    kond_min = min(mat.kond);
    spec_heat_max = max(mat.specific_heat);
    spec_heat_min = min(mat.specific_heat);
    density_max = max(mat.density);
    density_min = min(mat.density);
    
    lamba_max = (pi/spacing)^2 * kond_max/(spec_heat_max*density_max)
    dt_critical = 2/(1-2*theta) * 1/lamba_max
    
    if (dt > dt_critical)
      dt = .5*dt_critical;
    endif
  endif
    
  printf("Using dt = %f\n",dt);

  [p,e] = make_grid(bar,spacing);
  bcs = generate_bcs(bc,p);
  materials = create_materials_element(p,e,mat);
  T = set_intial_condition(initial_temp,length(p));

  
  t = 0;
  while (t <= end_time)
    [K,F] = initialize_K_F(length(p));
    [Keff,Feff] = form_matrix(K,F,p,e,materials,dt,theta,T);
    [Keff,Feff] = apply_bcs(Keff,Feff,bcs,materials);
    T = solve_system(Keff,Feff);
    xlabel("Bar points");
    ylabel("Temperature");
    plot(p,T)
    t += dt;
  endwhile
endfunction

