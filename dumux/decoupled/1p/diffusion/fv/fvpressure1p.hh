// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*****************************************************************************
 *   Copyright (C) 2008-2010 by Markus Wolff                                 *
 *   Copyright (C) 2007-2009 by Bernd Flemisch                               *
 *   Institute of Hydraulic Engineering                                      *
 *   University of Stuttgart, Germany                                        *
 *   email: <givenname>.<name>@iws.uni-stuttgart.de                          *
 *                                                                           *
 *   This program is free software: you can redistribute it and/or modify    *
 *   it under the terms of the GNU General Public License as published by    *
 *   the Free Software Foundation, either version 2 of the License, or       *
 *   (at your option) any later version.                                     *
 *                                                                           *
 *   This program is distributed in the hope that it will be useful,         *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 *   GNU General Public License for more details.                            *
 *                                                                           *
 *   You should have received a copy of the GNU General Public License       *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.   *
 *****************************************************************************/
#ifndef DUMUX_FVPRESSURE1P_HH
#define DUMUX_FVPRESSURE1P_HH


// dumux environment
#include <dumux/decoupled/1p/1pproperties.hh>

/**
 * @file
 * @brief  Single Phase Finite Volume Model
 * @author Markus Wolff
 */

namespace Dumux
{

//! \ingroup OnePhase
//! \brief Single Phase Finite Volume Model
/*! Provides a Finite Volume implementation for the evaluation
 * of equations of the form
 * \f[\text{div}\, \boldsymbol{v} = q.\f]
 * The velocity \f$\boldsymbol{v}\f$ is the single phase Darcy velocity:
 * \f[ \boldsymbol{v} = -\frac{1}{\mu} \boldsymbol{K} \left(\text{grad}\, p + \rho g  \text{grad}\, z\right), \f]
 * where \f$p\f$ is the pressure, \f$\boldsymbol{K}\f$ the absolute permeability, \f$\mu\f$ the viscosity, \f$\rho\f$ the density, and \f$g\f$ the gravity constant,
 * and \f$q\f$ is the source term.
 * At the boundary, \f$p = p_D\f$ on \f$\Gamma_{Dirichlet}\f$, and \f$\boldsymbol{v}_{total}  = q_N\f$
 * on \f$\Gamma_{Neumann}\f$.
 *
 * @tparam TypeTag The Type Tag
 *
 */
template<class TypeTag> class FVPressure1P
{
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(GridView)) GridView;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(Scalar)) Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(Problem)) Problem;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(Variables)) Variables;

    typedef typename GET_PROP_TYPE(TypeTag, PTAG(SpatialParameters)) SpatialParameters;

    typedef typename GET_PROP_TYPE(TypeTag, PTAG(Fluid)) Fluid;

    typedef typename GET_PROP_TYPE(TypeTag, PTAG(BoundaryTypes)) BoundaryTypes;
    typedef typename GET_PROP(TypeTag, PTAG(SolutionTypes)) SolutionTypes;
    typedef typename SolutionTypes::PrimaryVariables PrimaryVariables;

    enum
    {
        dim = GridView::dimension, dimWorld = GridView::dimensionworld
    };

    enum
    {
        pressEqIdx = 0 // only one equation!
    };

    typedef typename GridView::Traits::template Codim<0>::Entity Element;
    typedef typename GridView::template Codim<0>::Iterator ElementIterator;
    typedef typename GridView::Grid Grid;
    typedef typename GridView::template Codim<0>::EntityPointer ElementPointer;
    typedef typename GridView::IntersectionIterator IntersectionIterator;

    typedef Dune::FieldVector<Scalar, dimWorld> GlobalPosition;
    typedef Dune::FieldMatrix<Scalar, dim, dim> FieldMatrix;

    typedef typename GET_PROP_TYPE(TypeTag, PTAG(PressureCoefficientMatrix)) Matrix;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(PressureRHSVector)) Vector;

    //initializes the matrix to store the system of equations
    void initializeMatrix();

    //function which assembles the system of equations to be solved
    void assemble(bool first);

    //solves the system of equations to get the spatial distribution of the pressure
    void solve();

protected:
    //! Returns reference to the instance of the problem definition
    Problem& problem()
    {
        return problem_;
    }
    //! Returns reference to the instance of the problem definition
    const Problem& problem() const
    {
        return problem_;
    }

public:
    //! Initializes the problem
    /*!
     *  @param solveTwice repeats the pressure calculation step
     *
     *  Calculates the pressure \f$p\f$ as solution of the boundary value
     *  \f[  \text{div}\, \boldsymbol{v} = q, \f]
     *  subject to appropriate boundary conditions.
     */

    void initialize(bool solveTwice = true)
    {
        assemble(true);
        solve();
        if (solveTwice)
        {
            assemble(false);
            solve();
        }
        return;
    }

    //! Calculates the pressure.
    /*!
     *  @param solveTwice without any function here!
     *
     *  Calculates the pressure \f$p\f$ as solution of the boundary value
     *  \f[  \text{div}\, \boldsymbol{v} = q, \f]
     *  subject to appropriate boundary conditions.
     */
    void pressure(bool solveTwice = true)
    {
        assemble(false);
        solve();

        return;
    }

    // serialization methods
    //! Function needed for restart option.
    template<class Restarter>
    void serialize(Restarter &res)
    {
        return;
    }

    //! Function needed for restart option.
    template<class Restarter>
    void deserialize(Restarter &res)
    {
        return;
    }

    //! \brief Writes data files
    /*  \param writer VTK-Writer for the current simulation run */
    template<class MultiWriter>
    void addOutputVtkFields(MultiWriter &writer)
    {
        typename Variables::ScalarSolutionType *pressure = writer.allocateManagedBuffer (
                problem_.gridView().size(0));

        *pressure = problem_.variables().pressure();

        writer.attachCellData(*pressure, "pressure");

        return;
    }

    void setPressureHard(Scalar pressure, int globalIdx)
    {
        setPressHard_ = true;
        pressHard_ = pressure;
        idxPressHard_ = globalIdx;
    }

    void unsetPressureHard(int globalIdx)
    {
        setPressHard_ = false;
        pressHard_ = 0.0;
        idxPressHard_ = 0.0;
    }

    //! Constructs a FVPressure1P object
    /**
     * \param problem a problem class object
     */
    FVPressure1P(Problem& problem) :
        problem_(problem), A_(problem.variables().gridSize(), problem.variables().gridSize(), (2 * dim + 1)
                * problem.variables().gridSize(), Matrix::random), f_(problem.variables().gridSize()),
                pressHard_(0),
                idxPressHard_(0),
                setPressHard_(false),
                gravity(
                problem.gravity())
    {
        initializeMatrix();
    }

private:
    Problem& problem_;
    Matrix A_;
    Dune::BlockVector<Dune::FieldVector<Scalar, 1> > f_;
    Scalar pressHard_;
    Scalar idxPressHard_;
    bool setPressHard_;
protected:
    const GlobalPosition& gravity; //!< vector including the gravity constant
};

//!initializes the matrix to store the system of equations
template<class TypeTag>
void FVPressure1P<TypeTag>::initializeMatrix()
{
    // determine matrix row sizes
    ElementIterator eItEnd = problem_.gridView().template end<0> ();
    for (ElementIterator eIt = problem_.gridView().template begin<0> (); eIt != eItEnd; ++eIt)
    {
        // cell index
        int globalIdxI = problem_.variables().index(*eIt);

        // initialize row size
        int rowSize = 1;

        // run through all intersections with neighbors
        IntersectionIterator isItEnd = problem_.gridView().iend(*eIt);
        for (IntersectionIterator isIt = problem_.gridView().ibegin(*eIt); isIt != isItEnd; ++isIt)
            if (isIt->neighbor())
                rowSize++;
        A_.setrowsize(globalIdxI, rowSize);
    }
    A_.endrowsizes();

    // determine position of matrix entries
    for (ElementIterator eIt = problem_.gridView().template begin<0> (); eIt != eItEnd; ++eIt)
    {
        // cell index
        int globalIdxI = problem_.variables().index(*eIt);

        // add diagonal index
        A_.addindex(globalIdxI, globalIdxI);

        // run through all intersections with neighbors
        IntersectionIterator isItEnd = problem_.gridView().iend(*eIt);
        for (IntersectionIterator isIt = problem_.gridView().ibegin(*eIt); isIt != isItEnd; ++isIt)
            if (isIt->neighbor())
            {
                // access neighbor
                ElementPointer outside = isIt->outside();
                int globalIdxJ = problem_.variables().index(*outside);

                // add off diagonal index
                A_.addindex(globalIdxI, globalIdxJ);
            }
    }
    A_.endindices();

    return;
}

//!function which assembles the system of equations to be solved
template<class TypeTag>
void FVPressure1P<TypeTag>::assemble(bool first)
{
    // initialization: set matrix A_ to zero
    A_ = 0;
    f_ = 0;

    BoundaryTypes bcType;

    ElementIterator eItEnd = problem_.gridView().template end<0> ();
    for (ElementIterator eIt = problem_.gridView().template begin<0> (); eIt != eItEnd; ++eIt)
    {
        // get global coordinate of cell center
        const GlobalPosition& globalPos = eIt->geometry().center();

        // cell index
        int globalIdxI = problem_.variables().index(*eIt);

        // cell volume, assume linear map here
        Scalar volume = eIt->geometry().volume();

        Scalar temperatureI = problem_.temperature(*eIt);
        Scalar referencePressI = problem_.referencePressure(*eIt);

        Scalar densityI = Fluid::density(temperatureI, referencePressI);
        Scalar viscosityI = Fluid::viscosity(temperatureI, referencePressI);

        // set right side to zero
        PrimaryVariables source(0.0);
        problem_.source(source, *eIt);
        source /= densityI;

        f_[globalIdxI] = source *= volume;

        int isIndex = 0;
        IntersectionIterator isItEnd = problem_.gridView().iend(*eIt);
        for (IntersectionIterator isIt = problem_.gridView().ibegin(*eIt);
             isIt != isItEnd;
             ++isIt, ++isIndex)
        {
            // get normal vector
            Dune::FieldVector < Scalar, dimWorld > unitOuterNormal = isIt->centerUnitOuterNormal();

            // get face volume
            Scalar faceArea = isIt->geometry().volume();

            // handle interior face
            if (isIt->neighbor())
            {
                // access neighbor
                ElementPointer neighborPointer = isIt->outside();
                int globalIdxJ = problem_.variables().index(*neighborPointer);

                // neighbor cell center in global coordinates
                const GlobalPosition& globalPosNeighbor = neighborPointer->geometry().center();

                // distance vector between barycenters
                Dune::FieldVector < Scalar, dimWorld > distVec = globalPosNeighbor - globalPos;

                // compute distance between cell centers
                Scalar dist = distVec.two_norm();

                // compute vectorized permeabilities
                FieldMatrix meanPermeability(0);

                problem_.spatialParameters().meanK(meanPermeability,
                        problem_.spatialParameters().intrinsicPermeability(*eIt),
                        problem_.spatialParameters().intrinsicPermeability(*neighborPointer));

                Dune::FieldVector<Scalar, dim> permeability(0);
                meanPermeability.mv(unitOuterNormal, permeability);

                permeability/=viscosityI;

                Scalar temperatureJ = problem_.temperature(*neighborPointer);
                Scalar referencePressJ = problem_.referencePressure(*neighborPointer);

                Scalar densityJ = Fluid::density(temperatureJ, referencePressJ);

                Scalar rhoMean = 0.5 * (densityI + densityJ);

                // update diagonal entry
                Scalar entry;

                //calculate potential gradients
                Scalar potential = 0;

                Scalar density = 0;

                //if we are at the very first iteration we can't calculate phase potentials
                if (!first)
                {
                    potential = problem_.variables().potential(globalIdxI, isIndex);

                    density = (potential > 0.) ? densityI : densityJ;

                    density = (potential == 0.) ? rhoMean : density;

                    potential = (problem_.variables().pressure()[globalIdxI]
                            - problem_.variables().pressure()[globalIdxJ]) / dist;

                    potential += density * (unitOuterNormal * gravity);

                    //store potentials for further calculations (velocity, saturation, ...)
                    problem_.variables().potential(globalIdxI, isIndex) = potential;
                }

                //do the upwinding depending on the potentials

                density = (potential > 0.) ? densityI : densityJ;

                density = (potential == 0) ? rhoMean : density;

                //calculate current matrix entry
                entry = ((permeability * unitOuterNormal) / dist) * faceArea;

                //calculate right hand side
                Scalar rightEntry = density * (permeability * gravity) * faceArea;

                //set right hand side
                f_[globalIdxI] -= rightEntry;

                // set diagonal entry
                A_[globalIdxI][globalIdxI] += entry;

                // set off-diagonal entry
                A_[globalIdxI][globalIdxJ] = -entry;
            }

            // boundary face

            else if (isIt->boundary())
            {
                // center of face in global coordinates
                const GlobalPosition& globalPosFace = isIt->geometry().center();

                //get boundary condition for boundary face center
                problem_.boundaryTypes(bcType, *isIt);
                PrimaryVariables boundValues(0.0);

                if (bcType.isDirichlet(pressEqIdx))
                {
                    problem_.dirichlet(boundValues, *isIt);

                    Dune::FieldVector < Scalar, dimWorld > distVec(globalPosFace - globalPos);
                    Scalar dist = distVec.two_norm();

                    //permeability vector at boundary
                    // compute vectorized permeabilities
                    FieldMatrix meanPermeability(0);

                    problem_.spatialParameters().meanK(meanPermeability,
                            problem_.spatialParameters().intrinsicPermeability(*eIt));

                    //permeability vector at boundary
                    Dune::FieldVector < Scalar, dim > permeability(0);
                    meanPermeability.mv(unitOuterNormal, permeability);

                    permeability/= viscosityI;

                    //get dirichlet pressure boundary condition
                    Scalar pressBound = boundValues;

                    //calculate current matrix entry
                    Scalar entry = ((permeability * unitOuterNormal) / dist) * faceArea;

                    //calculate right hand side
                    Scalar rightEntry = densityI * (permeability * gravity) * faceArea;

                    // set diagonal entry and right hand side entry
                    A_[globalIdxI][globalIdxI] += entry;
                    f_[globalIdxI] += entry * pressBound;
                    f_[globalIdxI] -= rightEntry;
                }
                //set neumann boundary condition

                else if (bcType.isNeumann(pressEqIdx))
                {
                    problem_.neumann(boundValues, *isIt);
                    Scalar J = boundValues /= densityI;

                    f_[globalIdxI] -= J * faceArea;
                }
            }
        } // end all intersections
    } // end grid traversal
    return;
}

//!solves the system of equations to get the spatial distribution of the pressure
template<class TypeTag>
void FVPressure1P<TypeTag>::solve()
{
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(LinearSolver)) Solver;

    int verboseLevelSolver = GET_PARAM(TypeTag, int, LinearSolver, Verbosity);

    if (verboseLevelSolver)
        std::cout << "FVPressure1P: solve for pressure" << std::endl;

    if (setPressHard_)
    {
        A_[idxPressHard_] = 0;
        A_[idxPressHard_][idxPressHard_] = 1;
        f_[idxPressHard_] = pressHard_;
    }

    Solver solver(problem_);
    solver.solve(A_, problem_.variables().pressure(), f_);
    //                printmatrix(std::cout, A_, "global stiffness matrix", "row", 11, 3);
    //                printvector(std::cout, f_, "right hand side", "row", 200, 1, 3);
    //                printvector(std::cout, (problem_.variables().pressure()), "pressure", "row", 200, 1, 3);

    return;
}

}
#endif
