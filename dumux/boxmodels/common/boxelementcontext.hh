// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*****************************************************************************
 *   Copyright (C) 2010-2011 by Andreas Lauser                               *
 *   Institute for Modelling Hydraulic and Environmental Systems             *
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
/*!
 * \file
 * \ingroup BoxModel
 *
 * \brief Represents all quantities which are relevant for an element
 */
#ifndef DUMUX_BOX_ELEMENT_CONTEXT_HH
#define DUMUX_BOX_ELEMENT_CONTEXT_HH

#include "boxproperties.hh"

#include <dune/common/fvector.hh>

#include <vector>

namespace Dumux
{

/*!
 * \ingroup BoxModel
 *
 * \brief This class stores an array of VolumeVariables objects, one
 *        volume variables object for each of the element's vertices
 */
template<class TypeTag>
class BoxElementContext
{
    typedef typename GET_PROP_TYPE(TypeTag, PrimaryVariables) PrimaryVariables;
    typedef typename GET_PROP_TYPE(TypeTag, VolumeVariables) VolumeVariables;
    typedef typename GET_PROP_TYPE(TypeTag, FluxVariables) FluxVariables;

    // the history size of the time discretization in number of steps
    enum { timeDiscHistorySize = GET_PROP_VALUE(TypeTag, TimeDiscHistorySize) };

    struct ScvStore_ {
        VolumeVariables volVars[timeDiscHistorySize];
        PrimaryVariables priVars[timeDiscHistorySize];
        const VolumeVariables *hint[timeDiscHistorySize];
    };
    typedef std::vector<ScvStore_> ScvVarsVector;
    typedef std::vector<FluxVariables> ScvfVarsVector;

    typedef typename GET_PROP_TYPE(TypeTag, Problem) Problem;
    typedef typename GET_PROP_TYPE(TypeTag, Model) Model;
    typedef typename GET_PROP_TYPE(TypeTag, FVElementGeometry) FVElementGeometry;
    typedef typename GET_PROP_TYPE(TypeTag, SolutionVector) SolutionVector;
    typedef typename GET_PROP_TYPE(TypeTag, VertexMapper) VertexMapper;

    typedef typename GET_PROP_TYPE(TypeTag, GridView) GridView;
    typedef typename GridView::template Codim<0>::Entity Element;

    enum { dim = GridView::dimension };
    enum { numEq = GET_PROP_VALUE(TypeTag, NumEq) };

    typedef typename GridView::ctype CoordScalar;
    typedef Dune::FieldVector<CoordScalar, dim> GlobalPosition;

public:
    /*!
     * \brief The constructor.
     */
    explicit BoxElementContext(const Problem &problem)
        : gridView_(problem.gridView())
    {
        // remember the problem object
        problemPtr_ = &problem;
        modelPtr_ = &problem.model();
    }

    /*!
     * \brief Construct the volume variables of an element from scratch.
     *
     * \param problem The problem which needs to be simulated.
     * \param element The DUNE Codim<0> entity for which the volume variables ought to be calculated
     */
    void updateAll(const Element &elem)
    {
        updateFVElemGeom(elem);
        updateAllScvVars();
        updateAllScvfVars();
    }

    void updateFVElemGeom(const Element &elem)
    {
        // remember the current element
        elemPtr_ = &elem;

        // update the finite element geometry
        fvElemGeom_.update(gridView_, elem);

        // resize the SCV and the SCVF arrays
        if (fvElemGeom_.numVertices > scvVars_.size()) {
            scvVars_.resize(fvElemGeom_.numVertices);
        }
        if (fvElemGeom_.numEdges > scvfVars_.size())
            scvfVars_.resize(fvElemGeom_.numEdges);
    }

    void updateAllScvVars()
    {
       
        for (int timeIdx = 0; timeIdx < timeDiscHistorySize; ++ timeIdx)
            updateScvVars(timeIdx);
    };

    void updateScvVars(int timeIdx)
    {
        // update the volume variables for the whole history
        const VertexMapper &vertexMapper = problem().vertexMapper();
        const SolutionVector &globalSol = model().solution(timeIdx);

        int nScv = numScv();
        for (int scvIdx = 0; scvIdx < nScv; scvIdx++) {
            int globalIdx = vertexMapper.map(element(), scvIdx, dim);
            const PrimaryVariables &scvSol = globalSol[globalIdx];

            scvVars_[scvIdx].hint[timeIdx] = model().hint(globalIdx, timeIdx);
            updateScvVars(scvSol, scvIdx, timeIdx);
        }
    }

    void updateScvVars(const PrimaryVariables &priVars, int scvIdx, int timeIdx)
    {
        scvVars_[scvIdx].priVars[timeIdx] = priVars;
        scvVars_[scvIdx].volVars[timeIdx].update(/*context=*/*this, scvIdx, timeIdx);
    }

    void updateAllScvfVars()
    {
        scvfVarsEval_ = &scvfVars_;

        for (int scvfIdx = 0; scvfIdx < numScvf(); scvfIdx++) {
            scvfVars_[scvfIdx].update(/*context=*/ *this,
                                      /*localIndex=*/scvfIdx,
                                      /*timeIdx=*/0);
        }
    }

    /*!
     * \brief Return a reference to the problem.
     */
    const Problem &problem() const
    { return *problemPtr_; }

    /*!
     * \brief Return a reference to the model.
     */
    const Model &model() const
    { return *modelPtr_; }

    /*!
     * \brief Return a reference to the grid view.
     */
    const GridView &gridView() const
    { return gridView_; }

    /*!
     * \brief Return the current element.
     */
    const Element &element() const
    { return *elemPtr_; }

    /*!
     * \brief Return the number of sub-control volumes of the current element.
     */
    int numScv() const
    { return fvElemGeom_.numVertices; }

    /*!
     * \brief Return the number of sub-control volume faces of the current element.
     */
    int numScvf() const
    { return fvElemGeom_.numEdges; }

    /*!
     * \brief Return the current finite element geometry.
     */
    const FVElementGeometry &fvElemGeom(int timeIdx) const
    { return fvElemGeom_; }

    /*!
     * \brief Return the position of a local entities in global coordinates
     */
    const GlobalPosition &pos(int scvIdx, int timeIdx) const
    { return fvElemGeom_.subContVol[scvIdx].global; }

    /*!
     * \brief Return the global spatial index for a sub-control volume
     */
    int globalSpaceIndex(int scvIdx, int timeIdx) const
    { return model().vertexMapper().map(element(), scvIdx, dim); }

    /*!
     * \brief Returns whether the current element is on the domain's
     *        boundary.
     */
    bool onBoundary() const
    { return element().hasBoundaryIntersections(); };

    /*!
     * \brief Save the current flux variables and use them as the
     *        evaluation point.
     */
    void saveScvfVars()
    {
        scvfVarsSaved_ = scvfVars_;

        // change evaluation point
        scvfVarsEval_ = &scvfVarsSaved_;
    }

    /*!
     * \brief Restore current flux variables from the saved ones.
     */
    void restoreScvfVars()
    {
        //scvfVarsSaved_ = scvfVars_; // not needed

        // change evaluation point
        scvfVarsEval_ = &scvfVars_;
    }

    /*!
     * \brief Return a reference to the volume variables of a
     *        sub-control volume at a given time.
     *
     * If the time step index is not given, return the volume
     * variables for the current time.
     *
     * \param scvIdx The local index of the sub-control volume for
     *               which the volume variables are requested
     * \param timeIdx The index of the time step for which the
     *                    volume variables are requested. 0 means
     *                    current time step, 1 previous time step,
     *                    2 next-to-previous, etc.
     */
    const VolumeVariables &volVars(int scvIdx, int timeIdx) const
    { return scvVars_[scvIdx].volVars[timeIdx]; }

    const VolumeVariables *hint(int scvIdx, int timeIdx) const
    { return scvVars_[scvIdx].hint[timeIdx]; }
    /*!
     * \copydoc volVars()
     */
    VolumeVariables &volVars(int scvIdx, int timeIdx)
    { return scvVars_[scvIdx].volVars[timeIdx]; }

    PrimaryVariables &primaryVars(int scvIdx, int timeIdx)
    { return scvVars_[scvIdx].priVars[timeIdx]; }
    const PrimaryVariables &primaryVars(int scvIdx, int timeIdx) const
    { return scvVars_[scvIdx].priVars[timeIdx]; }

    /*!
     * \brief Returns the volume variables at the evaluation point.
     */
    void saveScvVars(int scvIdx)
    {
        scvIdxSaved_ = scvIdx;
        scvVarsSaved_ = scvVars_[scvIdx].volVars[/*timeIdx=*/0];
        priVarsSaved_ = scvVars_[scvIdx].priVars[/*timeIdx=*/0];
    }

    /*!
     * \brief Restores the volume variables at the evaluation point.
     */
    void restoreScvVars(int scvIdx)
    {
        scvIdxSaved_ = -1;
        scvVars_[scvIdx].priVars[/*timeIdx=*/0] = priVarsSaved_;
        scvVars_[scvIdx].volVars[/*timeIdx=*/0] = scvVarsSaved_;
    }

    /*!
     * \brief Return a reference to the flux variables of a
     *        sub-control volume face.
     *
     * \param scvfIdx The local index of the sub-control volume face for
     *               which the flux variables are requested
     */
    const FluxVariables &fluxVars(int scvIdx, int timeIdx) const
    { return scvfVars_[scvIdx]; }

    /*!
     * \brief Return a reference to the flux variables of a
     *        sub-control volume face for the evaluation point.
     *
     * \param scvfIdx The local index of the sub-control volume face for
     *               which the flux variables are requested
     */
    const FluxVariables &evalPointFluxVars(int scvfIdx, int timeIdx) const
    {
        if (timeIdx != 0)
            return fluxVars(scvfIdx, timeIdx);
        return (*scvfVarsEval_)[scvfIdx];
    }

    /*!
     * \brief Returns the volume variables for history index 0 at the
     *        evaluation point.
     */
    const VolumeVariables &evalPointVolVars(int scvIdx, int timeIdx) const
    {
        if (timeIdx != 0)
            return volVars(scvIdx, timeIdx);
        if (scvIdxSaved_ == scvIdx)
            return scvVarsSaved_;
        return volVars(scvIdx, /*timeIdx=*/0);
    }

protected:
    ScvVarsVector scvVars_;

    int scvIdxSaved_;
    VolumeVariables scvVarsSaved_;
    PrimaryVariables priVarsSaved_;

    ScvfVarsVector scvfVars_;
    ScvfVarsVector scvfVarsSaved_;

    ScvfVarsVector *scvfVarsEval_;

    const Problem *problemPtr_;
    const Model *modelPtr_;
    const Element *elemPtr_;
    const GridView gridView_;
    FVElementGeometry fvElemGeom_;
};

} // namespace Dumux

#endif