function [A,b,Alist] = setupInterface(grid,k,q,A,b,reallyUpdate)
%SETUPINTERFACE  Set the discrete operator at coarse-fine interface.
%   [A,B] = SETUPINTERFACE(GRID,K,Q,D,S,A,B) updates the LHS
%   matrix A and the RHS matrix B, adding to them all the equations at
%   coarse-fine interface on the coarse side (subtracting the original
%   coarse flux and adding instead hc/hf fine fluxes that use ghost
%   points). The effective ALPHA is 0.5 in all directions (see
%   SETUPPATCHBC).
%
%   See also: TESTDISC, ADDGRIDPATCH, SETUPPATCHBC.

globalParams;

if (param.verboseLevel >= 1)
    fprintf('--- setupPatchInterface(k = %d, q = %d) ---\n',k,q);
end

%=====================================================================
% Initialize; set fine patch "pointers" (in matlab: we actually copy P).
%=====================================================================
level                   = grid.level{k};
numPatches              = length(level.numPatches);
h                       = level.h;                                  % Fine meshsize h
P                       = grid.level{k}.patch{q};
ind                     = P.cellIndex;                              % Global 1D indices of cells
edgeDomain              = cell(2,1);                                % Domain edges
edgeDomain{1}           = level.minCell + P.offsetSub;              % First domain cell - next to left domain boundary - patch-based sub
edgeDomain{2}           = level.maxCell + P.offsetSub;              % Last domain cell - next to right domain boundary - patch-based sub
e                       = eye(grid.dim);

%=====================================================================
% Set coarse patch "pointers" (in matlab: we actually copy Q).
% Find Q-indices (interior,edge,BC) that lie under the fine patch and
% delete them (indDelete).
%=====================================================================
if (P.parent < 0)                                                 % Base patch at coarsest level, nothing to delete
    if (param.verboseLevel >= 2)
        fprintf('No parent patch\n');
    end
    return;
end
r                           = level.refRatio;                     % Refinement ratio H./h (H=coarse meshsize)
Qlevel                      = grid.level{k-1};
Q                           = Qlevel.patch{P.parent};             % Parent patch
QedgeDomain                 = cell(2,1);                          % Domain edges
QedgeDomain{1}              = Qlevel.minCell;                     % First domain cell - next to left domain boundary - patch-based sub
QedgeDomain{2}              = Qlevel.maxCell;                     % Last domain cell - next to right domain boundary - patch-based sub
QedgeDomain{:}

underLower                  = coarsenIndex(grid,k,P.ilower);      % level based sub
underUpper                  = coarsenIndex(grid,k,P.iupper);      % level based sub
underLower
underUpper

% underLower,underUpper are inside Q, so add to them BC vars whenever they
% are near the boundary.
lowerNearEdge               = find(underLower == QedgeDomain{1});
underLower(lowerNearEdge)   = underLower(lowerNearEdge) - 1;
upperNearEdge               = find(underUpper == QedgeDomain{2});
underUpper(upperNearEdge)   = underUpper(upperNearEdge) + 1;
underLower
underUpper

% Delete the equations at indDel. Note that there still remain connections
% from equations outside the deleted box to indDel variables.
%indDel                      = cell(grid.dim,1);
[indDel,del,matDel]         = indexBox(Q,underLower,underUpper);
A(indDel,indDel)            = eye(length(indDel));
b(indDel)                   = 0.0;
if (param.verboseLevel >= 3)
    indDel
end

% Delete remaining connections from outside the deleted box (indOut) to the
% deleted box (indDel).
[temp1,temp2,Alist] = setupPatchInterior(grid,k-1,P.parent,A,b,underLower,underUpper,0);
in2out              = Alist(~ismember(Alist(:,2),indDel),:);
out2in              = [in2out(:,2) in2out(:,2) -in2out(:,3)];
indOut              = unique(out2in(:,1));
Anew                = spconvert([out2in; [grid.totalVars grid.totalVars 0]]);
A(indOut,:)         = A(indOut,:) - Anew(indOut,:);

%=====================================================================
% Loop over all fine patch faces.
%=====================================================================
Alist                   = zeros(0,3);
indAll                  = [];
% Restore underLower,underUpper to exclude BC variables
underLower(lowerNearEdge)   = underLower(lowerNearEdge) - 1;
upperNearEdge               = find(underUpper == QedgeDomain{2});
underUpper(upperNearEdge)   = underUpper(upperNearEdge) + 1;

for d = 1:grid.dim,
    for s = [-1 1],
        if (param.verboseLevel >= 2)
            fprintf('  ==> (Fine Patch Face d = %d, s = %+d) ---\n',d,s);
        end
        dim = d;
        side = -s;                                                          % We look in the direction along the interface from the coarse patch into fine patch
        % Direction vector ("normal") from cell to its nbhr
        nbhrNormal      = zeros(1,grid.dim);
        nbhrNormal(dim) = side;
        sideNum         = (side+3)/2;                                       % side=-1 ==> 1; side=1 ==> 2
        fluxNum         = 2*dim+sideNum-2;
        otherDim        = setdiff(1:grid.dim,d)

        %=====================================================================
        % Prepare a list of all coarse and fine cell indices at this face.
        %=====================================================================
        % Coarse face variables
        if (    (underLower(d) == QedgeDomain{1}(d)) | ...
                (underLower(d) == QedgeDomain{2}(d)) )
            % This face is at the domain boundary, skip it
            continue;
        end
        Qilower                 = underLower;
        Qiupper                 = underUpper;
        if (s == -1)
            Qilower(d)          = Qilower(d)+s;             % To be outside the fine patch
            Qiupper(d)          = Qilower(d);
        else
            Qiupper(d)          = Qiupper(d)+s;             % To be outside the fine patch
            Qilower(d)          = Qiupper(d);
        end
        QboxSize                = Qiupper-Qilower+1;
        [indCoarse,coarse,matCoarse] = indexBox(Q,Qilower,Qiupper);

        % Fine face variables
        ilower                  = P.ilower;
        iupper                  = P.iupper;
        if (s == -1)
            iupper(d)           = ilower(d);
        else
            ilower(d)           = iupper(d);
        end
        boxSize                 = iupper-ilower+1;
        [indFine,fine,matFine]  = indexBox(P,ilower,iupper);

        %=====================================================================
        % Compute interpolation stencil of "ghost mirror" points m_i.
        %=====================================================================
        D                       = grid.dim-1;
        numPoints               = 2;
        points                  = [0:numPoints-1];              % 2nd order interpolation, lower left corner of stencil marked as (0,...,0) and is u_i (see comment below)

        % Compute interpolation points subscript offsets from u_i, where i
        % is the fine cell whose ghost mirror point will be interpolated
        % later. The same stencil holds for all i.
        temp                    = cell(D,1);
        [temp{:}]               = ind2sub(2*ones(D,1),[1:2^D]);
        numInterp               = length(temp{1});
        subInterp               = zeros(numInterp,D);
        for other = 1:D
            subInterp(:,other)  = temp{other}';
        end
        subInterp

        % Compute barycentric interpolation weights of points to mirror
        % m (separate for each dimension; dimension other weights
        % are w(other,:))

        % Barycentric weights, independent of m, stored in w
        w = zeros(D,numPoints);
        for other = 1:D
            w(other,1)  = 1.0;
            for j = 2:numPoints
                w(other,1:j-1)  = (points(1:j-1) - points(j)).*w(D,1:j-1);
                w(other,j)      = prod(points(j) - points(1:j-1));
            end
        end
        w       = 1.0./w;

        % interpolation weights from points to m in dimension other,
        % stored in w
        a = (r(d)-1)/(r(d)+1);
        m = ((a-1)/(2*a))*ones(D,1);                % Mirror relative location
        for other = 1:D
            w(other,:) = w(other,:)./(m(other) - points);
            w(other,:) = w(other,:)./sum(w(other,:));
        end

        % Tensor product of 1D interpolation stencils to obtain the full
        % D-dimensional interpolation stencil from points to m
        wInterp                 = ones(numInterp,1);
        for other = 1:D,
            wInterp             = wInterp .* w(other,subInterp(:,other))';
        end
        wInterp

        % Loop over different types of fine cells with respect to a coarse
        % cell (there are 2^D types) and add connections to Alist.
        temp                    = cell(D,1);
        [temp{:}]               = ind2sub(r(otherDim),[1:prod(r(otherDim))]);
        numChilds               = length(temp{1});
        subChilds               = zeros(numChilds,D);
        for dim = 1:D
            subChilds(:,dim)    = temp{dim}' - 1;
        end
        subChilds
        matCoarseNbhr           = matCoarse;
        matCoarseNbhr{d}        = matCoarse{d} - s;
        matCoarseNbhr{:}
        colCoarseNbhr           = cell2mat(matCoarseNbhr)';
        colCoarseNbhr           = colCoarseNbhr ...
            - repmat(Q.offsetSub,size(colCoarseNbhr)./size(Q.offsetSub))
        childBase               = refineIndex(grid,k-1,colCoarseNbhr)
        j                       = zeros(1,grid.dim);
        jump                    = zeros(1,grid.dim);
        for t = 1:numChilds,
            fprintf('------------------- t = %d ---------------\n',t);
            %=====================================================================
            % Create a list of non-zeros to be added to A, consisting of the
            % connections of type-t-fine-cells at this face to their coarse
            % counterparts.
            %=====================================================================
            j(otherDim)     = subChilds(t,:);
            j(d)            = 0;
            jump            = r-1-2*j;
            jump(d)         = 0;

            subInterpFine   = zeros(numInterp,grid.dim)
            subInterpFine(:,otherDim) = (subInterp-1)
            subInterpFine   = subInterpFine .* repmat(jump,size(subInterpFine)./size(jump))
            dupInterpFine   = repmat(subInterpFine,[size(childBase,1),1])

            childBase            
            dupChildBase    = reshape(repmat((childBase(:))',[size(subInterpFine,1),1]),[size(dupInterpFine,1) grid.dim]);
            dupInterpFine   = dupChildBase + dupInterpFine;
            dupwInterp      = repmat(wInterp,[size(childBase,1),1])
            matInterpFine   = mat2cell(dupInterpFine,size(dupInterpFine,1),[1 1]);
            indInterpFine   = ind(sub2ind(P.size,matInterpFine{:}));
            indInterpFine   = indInterpFine(:);

            wInterp
            a

            thisChild       = childBase + repmat(j,size(childBase)./size(j))
            matThisChild    = mat2cell(thisChild,size(thisChild,1),[1 1]);
            indThisChild    = ind(sub2ind(P.size,matThisChild{:}));
            indThisChild    = indThisChild(:);
            dupThisChild    = reshape(repmat((thisChild(:))',[size(subInterpFine,1),1]),[size(dupInterpFine,1) grid.dim]);
            matDupThisChild    = mat2cell(dupThisChild,size(dupThisChild,1),[1 1]);
            indDupThisChild    = ind(sub2ind(P.size,matDupThisChild{:}));
            indDupThisChild    = indDupThisChild(:);
            indGhost        = indexNbhr(P,indThisChild,-nbhrNormal)

            % Create ghost equations
            Alist = [Alist; ...                                                 % We are never near boundaries according to the C/F interface existence rules
                [indGhost       indGhost             repmat(1.0/(a-1.0),size(indGhost))]; ...
                [indGhost       indCoarse            repmat(1.0,size(indGhost))]; ...
                [indGhost       indThisChild         repmat(-1.0,size(indGhost))] ...
                ];

            % Add (flux-based) ghost points to coarse equations
            Alist = [Alist; ...                                                 % We are never near boundaries according to the C/F interface existence rules
                [indCoarse      indGhost             repmat(-1.0,size(indGhost))]; ...
                ];

            % Add correction terms to fine grid equations due to 
            % the linear interpolation (actually extrapolation) scheme 
            % of ghost points. For constant interpolation of ghosts, there
            % are no correction terms.

            Alist = [Alist; ...                                                 % We are never near boundaries according to the C/F interface existence rules
                [indThisChild   indGhost             repmat(1.0,size(indGhost))]; ... % Ghost flux term
                [indThisChild   indThisChild         repmat(a,size(indGhost))]; ... % Self-term (results from ui in the definition of the ghost flux = ui-gi+a*(mirrorGhostInterpTerms)
                [indThisChild   indThisChild         repmat(a,size(indGhost))]; ... % Self-term (results from ui in the definition of the ghost flux = ui-gi+a*(mirrorGhostInterpTerms)                
                [indDupThisChild indInterpFine      -a*dupwInterp]; ...             % Interpolation terms
                ];

            % Add C,F,ghost nodes to global index list for the A-update
            % following this loop.
            indAll          = union(indAll,indCoarse);
            indAll          = union(indAll,indThisChild);
            indAll          = union(indAll,indGhost);
        end

    end
end

if (reallyUpdate)
    %=====================================================================
    % Add the links above to the relevant equations (rows) in A.
    %=====================================================================
    Anew                = spconvert([Alist; [grid.totalVars grid.totalVars 0]]);
    A(indAll,:)         = A(indAll,:) + Anew(indAll,:);                       % Do not replace the non-zeros in A here, rather add to them.
end
