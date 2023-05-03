"""
Wrapper class for RunNode that adds methods to plot and access results.

The motivation for this class is following. Consider that user wants to plot
a figure that shows i) particle orbits in (R,z), ii) their final (R,z) position,
and iii) the wall tiles. It is not clear whether this plot should be implemented
as a method for State or Orbit class as data from both are needed. Furthermore,
neither has access to the wall data. The only logical place for this method
therefore is in the RunNode that has access to all relevant data.

File: run.py
"""

import numpy as np
import matplotlib.pyplot as plt
import pyvista as pv

import a5py.marker.endcond as endcondmod
import a5py.plotting as a5plt
import a5py.wall as wall

class LackingDataError(Exception):
    """
    Raised when the HDF5 file does not have necessary groups for the operation.
    """

    def __init__(self, missing):
        """
        Error informing what groups is missing.

        Args:
            missing : str <br>
                Name of the group that is missing.
        """
        msg = "Output does not contain " + missing + " which is required.\n"
        super().__init__(msg)


class RunMethods():

    def init_runmethods(self):
        self.has_inistate = False
        self.has_endstate = False
        self.has_orbit    = False

        if hasattr(self, "inistate"):
            self.has_inistate = True
        if hasattr(self, "endstate"):
            self.has_endstate = True
        if hasattr(self, "orbit"):
            self.has_orbit = True


    def getstate(self):
        pass


    def getorbit(self):
        if not has_orbit:
            pass


    def getstate_markersummary(self):
        """
        Returns a summary of marker endconds.

        Returns:
            summary : array_like, str <br>
                Array of strings where each string has a format
                "# of markers : end condition". If markers were aborted
                then also error messages are listed as separate strings.
        Raise:
            LackingDataError if queried endstate is missing from output.
        """
        if not self.has_endstate:
            raise LackingDataError("endstate")
        econd = self.endstate["endcond"]
        emsg  = self.endstate["errormsg"]
        emod  = self.endstate["errormod"]
        eline = self.endstate["errorline"]
        errors = np.unique(np.array([emsg, eline, emod]), axis=1).T

        ec, counts = np.unique(econd, return_counts=True)
        msg = ["End conditions:"]
        for i, e in enumerate(ec):
            econd = endcondmod.getname(e)
            if econd == "none": econd = "aborted"
            msg.append(str(counts[i]) + " : " + econd)

        modules = [
            "mccc_wiener.c", "mccc_push.c", "mccc.c", "step_fo_vpa.c",
            "step_gc_cashkarp.c", "step_gc_rk4", "N0_3D.c", "N0_ST.c",
            "B_2DS.c", "B_2DS.c", "B_STS.c", "B_GS.c", "plasma_1D.c",
            "plasma_1DS.c", "plasma.c", "E_field.c", "neutral.c",
            "E_1DS.c", "B_field.c", "particle.c", "boozer.c", "mhd.c"
        ]

        messages = [
            "Input evaluation failed", "Unknown input type",
            "Unphysical quantity when evaluating input",
            "Unphysical marker quantity", "Time step too small/zero/NaN",
            "Wiener array is full or inconsistent",
            "Unphysical result when integrating marker coordinates"
        ]

        for e in errors:
            if np.sum(e) > 0:
                msg.append("Error: " + messages[e[0]-1] \
                           + ". At line " + str(e[1])  \
                           + " in " + modules[e[2]-1])

        return msg


    def getstate_losssummary(self):
        """
        """
        if not self.has_endstate:
            raise LackingDataError("endstate")

        wmrks = self.endstate.get("weight")
        wloss = self.endstate.get("weight", endcond="wall")
        emrks = self.endstate.get("energy")
        eloss = self.endstate.get("energy", endcond="wall")

        markers_lost   = wloss.size
        markers_frac   = wloss.size / wmrks.size
        particles_lost = np.sum(wloss)
        particles_frac = np.sum(wloss) / np.sum(wmrks)
        power_lost     = np.sum(wloss * eloss)
        power_frac     = np.sum(wloss * eloss) / np.sum(wmrks * emrks)

        msg = []
        msg += ["Markers lost: " + str(markers_lost) + " ("
                + str(np.around(markers_frac*100, decimals=1)) + "% of total)"]
        msg += ["Particles lost: " + str(particles_lost) + " ("
                + str(np.around(particles_frac*100, decimals=1)) + "% of total)"]
        msg += ["Energy lost: " + str(power_lost) + " ("
                + str(np.around(power_frac*100, decimals=1)) + "% of total)"]
        return msg


    def getwall_figuresofmerit(self):
        ids    = self.endstate.get("walltile", endcond="wall")
        energy = self.endstate.get("energy", endcond="wall")
        weight = self.endstate.get("weight", endcond="wall")
        area   = self.wall.area()

        wetted_total, energy_peak = wall.figuresofmerit(ids, energy, weight, area)
        unit = "J"
        if energy_peak > 1e9:
            energy_peak /= 1e9
            unit = "GJ"
        elif energy_peak > 1e6:
            energy_peak /= 1e6
            unit = "MJ"
        elif energy_peak > 1e3:
            energy_peak /= 1e3
            unit = "KJ"

        msg = []
        msg += ["Total wetted area: " + str(np.around(wetted_total, decimals=2)) + r" $m^2$"]
        msg += ["Peak load: " + str(np.around(energy_peak, decimals=2)) + " " + str(unit) + r"$/m^2$"]
        return msg


    def plotwall_loadvsarea(self, axes=None):
        ids, _, eload, _, _, _, _ = self.getwall_loads()
        area = self.wall.area()[ids-1]
        a5plt.loadvsarea(area, eload, axes=axes)


    def getorbit_poincareplanes(self):
        """
        Return a list of Poincaré planes that was collected in the simulation.

        The list consists of "pol", "tor", and "rad" depending on the type of
        the plane.
        """
        opt  = self.options.read()
        npol = opt["ORBITWRITE_POLOIDALANGLES"]
        ntor = opt["ORBITWRITE_TOROIDALANGLES"]
        nrad = opt["ORBITWRITE_RADIALDISTANCES"]
        npol = 0 if npol[0] < 0 else npol.size
        ntor = 0 if ntor[0] < 0 else ntor.size
        nrad = 0 if nrad[0] < 0 else nrad.size

        plane = [None] * (npol + ntor + nrad)
        plane[:npol]                    = ["pol"]
        plane[npol:npol+ntor]           = ["tor"]
        plane[npol+ntor:npol+ntor+nrad] = ["rad"]

        return plane


    def plotstate_scatter(
            self, x, y, z=None, c=None, color="C0", endcond=None,
            iniend=["i", "i", "i", "i"], log=[False, False, False, False],
            axesequal=False, axes=None, cax=None):
        """
        Make a scatter plot of marker state coordinates.

        Marker ini and end state contains the information of marker phase-space
        position at the beginning and at the end of the simulation. With this
        routine one can plot e.g. final (R,z) coordinates or mileage vs initial
        rho coordinate.

        The plot is either 2D+1D or 3D+1D, where the extra coordinate is color,
        depending on the number of queried coordinates.

        Args:
            x : str <br>
                Name of the quantity on x-axis.
            y : str <br>
                Name of the quantity on y-axis.
            z : str, optional <br>
                Name of the quantity on z-axis (makes plot 3D if given).
            c : str, optional <br>
                Name of the quantity shown with color scale.
            color : str, optional <br>
                Name of the color markers are colored with if c is not given.
            nc : int, optional <br>
                Number of colors used if c is given (the color scale is not
                continuous)
            cmap : str, optional <br>
                Name of the colormap where nc colors are picked if c is given.
            endcond : str, array_like, optional <br>
                Endcond of those  markers which are plotted.
            iniend : str, array_like, optional <br>
                Flag whether a corresponding [x, y, z, c] coordinate is taken
                from the ini ("i") or endstate ("e").
            log : bool, array_like, optional <br>
                Flag whether the corresponding axis [x, y, z, c] is made
                logarithmic. If that is the case, absolute value is taken before
                the quantity is passed to log10.
            axesequal : bool, optional <br>
                Flag whether to set aspect ratio for x and y (and z) axes equal.
            axes : Axes, optional <br>
                The Axes object to draw on. If None, a new figure is displayed.
            cax : Axes, optional <br>
                The Axes object for the color data (if c contains data),
                otherwise taken from axes.
        Raise:
            LackingDataError if queried state is missing from output.
        """
        if not self.has_inistate and "i" in iniend:
            raise LackingDataError("inistate")
        if not self.has_endstate and "e" in iniend:
            raise LackingDataError("endstate")

        # Decipher whether quantity is evaluated from ini or endstate
        state = [None] * 4
        for i in range(4):
            if iniend[i] == "i":
                state[i] = self.inistate
            elif iniend[i] == "e":
                state[i] = self.endstate
            else:
                raise ValueError("Only 'i' and 'e' are accepted for iniend.")

        # Function for processing given coordinate
        def process(crdname, crdindex):

            # Read data
            crd   = state[crdindex].get(crdname, endcond=endcond)
            label = crdname + " [" + str(crd.units) + "]"

            return (crd, label)


        # Process
        xc, xlabel = process(x, 0)
        yc, ylabel = process(y, 1)
        if z is not None:
            zc, zlabel = process(z, 2)
        if c is not None:
            cc, clabel = process(c, 3)
        else:
            cc = color
            clabel = None

        # Plot
        if z is None:
            a5plt.scatter2d(x=xc, y=yc, c=cc, log=[log[0], log[1], log[3]],
                            xlabel=xlabel, ylabel=ylabel, clabel=clabel,
                            axesequal=axesequal, axes=axes, cax=cax)
        else:
            a5plt.scatter3d(x=xc, y=yc, z=zc, c=cc, log=log, xlabel=xlabel,
                            ylabel=ylabel, zlabel=zlabel, clabel=clabel,
                            axesequal=axesequal, axes=axes, cax=cax)


    def plotstate_histogram(
            self, x, y=None, xbins=None, ybins=None, endcond=None, weight=False,
            iniend=["i", "i"], log=[False, False], logscale=False,
            cmap="viridis", axesequal=False, axes=None, cax=None):
        """
        Make a histogram plot of marker state coordinates.

        The histogram is either 1D or 2D depending on if the y coordinate is
        provided. In the 1D histogram the markers with different endstate are
        separated by color if the endcond argument is None.

        Args:
            x : str <br>
                Name of the quantity on x-axis.
            y : str, optional <br>
                Name of the quantity on y-axis. Makes the histogram 2D.
            cmap : str, optional <br>
                Name of the colormap used in the 2D histogram.
            endcond : str, array_like, optional <br>
                Endcond of those  markers which are plotted. Separated by color
                in 1D plot otherwise.
            iniend : str, array_like, optional <br>
                Flag whether a corresponding [x, y] coordinate is taken from
                the ini ("i") or endstate ("e").
            log : bool, array_like, optional <br>
                Flag whether the corresponding axis [x, y] is made logarithmic.
                If that is the case, absolute value is taken before the quantity
                is passed to log10.
            axesequal : bool, optional <br>
                Flag whether to set aspect ratio for x and y axes equal in 2D.
            axes : Axes, optional <br>
                The Axes object to draw on. If None, a new figure is displayed.
            cax : Axes, optional <br>
                The Axes object for the color data in 2D, otherwise taken from
                axes.
        Raise:
            LackingDataError if queried state is missing from output.
        """
        if not self.has_inistate and "i" in iniend:
            raise LackingDataError("inistate")
        if not self.has_endstate and "e" in iniend:
            raise LackingDataError("endstate")
        if not self.has_endstate and endcond is not None:
            raise LackingDataError("endstate")

        # Decipher whether quantity is evaluated from ini or endstate
        state = [None] * 2
        for i in range(2):
            if iniend[i] == "i":
                state[i] = self.inistate
            elif iniend[i] == "e":
                state[i] = self.endstate
            else:
                raise ValueError("Only 'i' and 'e' are accepted for iniend.")

        if y is None:
            # 1D plot

            xc = []
            weights = []

            ecs, count = self.endstate.listendconds()
            for ec in ecs:
                if endcond is not None and ec not in [endcond]:
                    pass

                xc.append(state[0].get(x, endcond=ec))
                weights.append(state[0].get("weight", endcond=ec))

            idx = np.argsort([len(i) for i in xc])[::-1]
            xc  = [xc[i] for i in idx]
            ecs = [ecs[i] for i in idx]
            weights = [weights[i] for i in idx]
            if not weight:
                weights = None

            xlabel = x + " [" + str(xc[0].units) + "]"
            a5plt.hist1d(x=xc, xbins=xbins, weights=weights,
                         log=[log[0], logscale],
                         xlabel=xlabel, axes=axes, legend=ecs)

        else:
            # 2D plot
            xc = state[0].get(x, endcond=endcond)
            yc = state[1].get(y, endcond=endcond)
            weights = state[0].get("weight", endcond=endcond)
            if not weight:
                weights = None

            xlabel = x + " [" + str(xc.units) + "]"
            ylabel = y + " [" + str(yc.units) + "]"
            a5plt.hist2d(xc, yc, xbins=xbins, ybins=ybins, weights=weights,
                         log=[log[0], log[1], logscale], xlabel=xlabel,
                         ylabel=ylabel, axesequal=axesequal, axes=axes, cax=cax)


    def plotorbit_poincare(self, plane, conlen=True, axes=None, cax=None):
        """
        Poincaré plot where color separates markers or shows connection length.

        Args:
            plane : int <br>
                Index number of the plane to be plotted in the list given by
                getorbit_poincareplanes.
            conlen : bool, optional <br>
                If true, trajectories of lost markers are colored (in blue
                shades) where the color shows the connection length at that
                position. Confined (or all if conlen=False) markers are shown
                with shades of red where color separates subsequent
                trajectories.
            axes : Axes, optional <br>
                The Axes object to draw on.
            cax : Axes, optional <br>
                The Axes object for the connection length (if conlen is True),
                otherwise taken from axes.
        Raise:
            LackingDataError if endstate or poincaré data is missing from
            output.
        """
        if not self.has_orbit:
            raise LackingDataError("orbit")
        try:
            self.orbit["pncrid"]
        except:
            raise LackingDataError("orbit(poincare)")
        if not self.has_endstate:
            raise LackingDataError("endstate")

        # Set quantities corresponding to the planetype
        planes = self.getorbit_poincareplanes()
        if len(planes) <= plane:
            raise ValueError("Unknown plane.")

        ptype = planes[plane]
        if ptype == "pol":
            x = "r"
            y = "z"
            xlabel = "R [m]"
            ylabel = "z [m]"
            xlim = None # TBD
            ylim = None # TBD
            axesequal = True
        elif ptype == "tor":
            x = "rho"
            y = "phimod"
            xlabel = "Normalized poloidal flux"
            ylabel = "Toroidal angle [deg]"
            xlim = [0, 1.1]
            ylim = [0, 360]
            axesequal = False
        elif ptype == "rad":
            x = "thetamod"
            y = "phimod"
            xlabel = "Poloidal angle [deg]"
            ylabel = "Toroidal angle [deg]"
            xlim = [0, 360]
            ylim = [0, 360]
            axesequal = True

        ids = self.orbit.get("id", pncrid=plane)
        x   = self.orbit.get(x,    pncrid=plane)
        y   = self.orbit.get(y,    pncrid=plane)

        if ptype == "pol":
            # Determine (R,z) limits by making sure the data fits nicely and
            # then rounding to nearest decimal.
            xlim = [np.floor(np.amin(x)*9) / 10, np.ceil(np.amax(x)*11) / 10]
            ylim = [np.floor(np.amin(y)*11) / 10, np.ceil(np.amax(y)*11) / 10]

        if conlen:
            # Calculate connection length using endstate (only markers that pass
            # this plane are considered)
            total = self.endstate.get("mileage", ids=np.unique(ids))

            # Find how many data points each marker has in orbit data arrays.
            # idx are indices of last data point for given id.
            idx = np.argwhere(ids[1:] - ids[:-1]).ravel()
            idx = np.append(idx, ids.size-1)
            # Getting the number of data points now is just a simple operation:
            idx[1:] -= idx[:-1]
            idx[0] += 1

            # Now we can repeat the total mileage by the number of data points
            # each marker has, so we can calculate the connection length at each
            # point as: conlen = total - mileage
            total  = np.repeat(total, idx)
            conlen = total - self.orbit.get("mileage", pncrid=plane)

            # Now set confined markers as having negative connection length
            lost1 = self.endstate.get("id", endcond="rhomax")
            lost2 = self.endstate.get("id", endcond="wall")

            idx = ~np.logical_or(np.in1d(ids, lost1), np.in1d(ids, lost2))
            conlen[idx] *= -1
            clabel = "Connection length [" + str(conlen.units) + "]"
        else:
            conlen = None
            clabel = None

        a5plt.poincare(x, y, ids, conlen=conlen, xlim=xlim, ylim=ylim,
                       xlabel=xlabel, ylabel=ylabel, clabel=clabel,
                       axesequal=axesequal, axes=axes, cax=cax)


    def plotstate_summary(self, axes_inirho=None, axes_endrho=None,
                          axes_mileage=None, axes_energy=None,
                          axes_rz=None, axes_rhophi=None):
        """
        Plot several graphs that summarize the simulation.

        Following graphs are plotted:
          - inirho: Initial rho histogram with colors marking the endcond.
          - endrho: Initial rho histogram with colors marking the endcond.
          - mileage: Final mileage histogram with colors marking the endcond.
          - energy: Final energy histogram with colors marking the endcond.
          - Rz: Final R-z scatterplot.
          - rhophi: Final rho-phi scatterplot.

        Args:
            axes_inirho: {Axes, bool}, optional <br>
                Axes where inirho is plotted else a new figure is created. If
                False, then this plot is omitted.
            axes_endrho: {Axes, bool}, optional <br>
                Axes where inirho is plotted else a new figure is created. If
                False, then this plot is omitted.
            axes_mileage: {Axes, bool}, optional <br>
                Axes where inirho is plotted else a new figure is created. If
                False, then this plot is omitted.
            axes_energy: {Axes, bool}, optional <br>
                Axes where inirho is plotted else a new figure is created. If
                False, then this plot is omitted.
            axes_rz: {Axes, bool}, optional <br>
                Axes where inirho is plotted else a new figure is created. If
                False, then this plot is omitted.
            axes_rhophi: {Axes, bool}, optional <br>
                Axes where inirho is plotted else a new figure is created. If
                False, then this plot is omitted.
        """

        # Initial rho histogram with colors marking endcond
        fig = None
        if axes_inirho is None:
            fig = plt.figure()
            axes_inirho = fig.add_subplot(1,1,1)
        if axes_inirho != False:
            axes_inirho.set_xlim(0,1.1)
            axes_inirho.set_title("Initial radial position")
            self.plotstate_histogram(
                "rho", xbins=np.linspace(0,1.1,55), weight=True,
                iniend=["i", "i"], axes=axes_inirho)
            if fig is not None: plt.show()

        # Final rho histogram with colors marking endcond
        fig = None
        if axes_endrho is None:
            fig = plt.figure()
            axes_endrho = fig.add_subplot(1,1,1)
        if axes_endrho != False:
            axes_endrho.set_xlim([0,1.1])
            axes_endrho.set_title("Final radial position")
            self.plotstate_histogram(
                "rho", xbins=np.linspace(0,1.1,55), weight=True,
                iniend=["e", "i"], axes=axes_endrho)
            if fig is not None: plt.show()

        # Mileage histogram with colors marking endcond
        fig = None
        if axes_mileage is None:
            fig = plt.figure()
            axes_mileage = fig.add_subplot(1,1,1)
        if axes_mileage != False:
            axes_mileage.set_title("Final mileage")
            self.plotstate_histogram(
                "mileage", xbins=55, weight=True,
                iniend=["e", "i"], log=[True, False], axes=axes_mileage)
            if fig is not None: plt.show()

        # Final energy histogram with colors marking endcond
        fig = None
        if axes_energy is None:
            fig = plt.figure()
            axes_energy = fig.add_subplot(1,1,1)
        if axes_energy != False:
            axes_energy.set_title("Final energy")
            self.plotstate_histogram(
                "energy", xbins=55, weight=True,
                iniend=["e", "i"], log=[True, False], axes=axes_energy)
            if fig is not None: plt.show()

        # Final Rz scatter positions
        fig = None
        if axes_rz is None:
            fig = plt.figure()
            axes_rz = fig.add_subplot(1,1,1)
        if axes_rz != False:
            axes_rz.set_title("Final R-z positions")
            self.plotstate_scatter(
                "R", "z", color="C0", endcond=None,
                iniend=["e", "e", "i", "i"], axesequal=True, axes=axes_rz)
            if fig is not None: plt.show()

        # Final rho-phi scatter positions
        fig = None
        if axes_rhophi is None:
            fig = plt.figure()
            axes_rhophi = fig.add_subplot(1,1,1)
        if axes_rhophi != False:
            axes_rhophi.set_xlim(left=0)
            axes_rhophi.set_ylim([0,360])
            axes_rhophi.set_xticks([0, 0.5, 1.0])
            axes_rhophi.set_yticks([0, 180, 360])
            axes_rhophi.set_title("Final rho-phi positions")
            self.plotstate_scatter(
                "rho", "phimod", color="C0", endcond=None,
                iniend=["e", "e", "i", "i"], axesequal=False, axes=axes_rhophi)
            if fig is not None: plt.show()


    def getstate_pointcloud(self, endcond=None):
        """
        Return marker endstate (x,y,z) coordinates in single array.

        Args:
            endcond : str, optional <br>
                Only return markers that have given end condition.
        """
        if not self.has_endstate:
            raise LackingDataError("endstate")
        return np.array([self.endstate.get("x", endcond=endcond),
                         self.endstate.get("y", endcond=endcond),
                         self.endstate.get("z", endcond=endcond)]).T

    def getwall_loads(self):
        """
        Get 3D wall loads and associated quantities.

        This method does not return loads on all wall elements (as usually most
        of them receive no loads) but only those that are affected and their
        IDs.

        Returns:
            ids
            edepo
            eload
            pdepo
            pload
            mdepo
            iangle
        """
        ids    = self.endstate.get("walltile", endcond="wall")
        energy = self.endstate.get("energy", endcond="wall")
        weight = self.endstate.get("weight", endcond="wall")
        area   = self.wall.area()
        return wall.loads(ids, energy, weight, area)


    def getwall_3dmesh(self):
        """
        Return 3D mesh representation of 3D wall and associated loads.

        Returns:
            wallmesh : Polydata <br>
                Mesh representing the wall. Cell data has fields
                "pload" (particle load in units of prt/m^2 or prt/m^2s),
                "eload" (power/energy load in units of W/m^2 or J/m^2),
                and "iangle" (angle of incidence in units of deg).
        """
        wallmesh = pv.PolyData( *self.wall.noderepresentation() )
        ids, _, eload, _, pload, _, iangle = self.getwall_loads()
        ids = ids - 1 # Convert IDs to indices
        ntriangle = wallmesh.n_faces
        wallmesh.cell_data["pload"]       = np.zeros((ntriangle,)) + np.nan
        wallmesh.cell_data["pload"][ids]  = pload
        wallmesh.cell_data["eload"]       = np.zeros((ntriangle,)) + np.nan
        wallmesh.cell_data["eload"][ids]  = pload
        wallmesh.cell_data["iangle"]      = np.zeros((ntriangle,)) + np.nan
        wallmesh.cell_data["iangle"][ids] = iangle
        return wallmesh


    def plotwall_3dstill(self, wallmesh=None, points=None, data=None, log=False,
                         cpos=None, cfoc=None, cang=None, axes=None, cax=None):
        """
        Take a still shot of the mesh and display it using matplotlib backend.

        The rendering is done using vtk but the vtk (interactive) window is not
        displayed. It is recommended to use the interactive plot to find desired
        camera position and produce the actual plot using this method. The plot
        is shown using imshow and acts as a regular matplotlib plot.

        Args:
            wallmesh : Polydata <br>
                Mesh representing the wall. If not given then construct the
                mesh from the wall data.
            points : {array_like, bool}, optional <br>
                Array Npoint x 3 defining points (markers) to be shown. For
                each point [x, y, z] coordinates are given. If boolean True is
                given, then markers are read from the endstate.
            cpos : array_like, optional <br>
                Camera position coordinates [x, y, z].
            cfoc : array_like, optional <br>
                Camera focal point coordinates [x, y, z].
            cang : array_like, optional <br>
                Camera angle [azimuth, elevation, roll].
            axes : Axes, optional <br>
                The Axes object to draw on.
            cax : Axes, optional <br>
                The Axes object for the color data (if c contains data),
                otherwise taken from axes.
        """
        if wallmesh is None:
            wallmesh = self.getwall_3dmesh()
        if isinstance(points, bool) and points == True:
            points = self.getstate_pointcloud(endcond="wall")

        (cpos0, cfoc0, cang0) = a5plt.defaultcamera(wallmesh)
        if cpos is None: cpos = cpos0
        if cfoc is None: cfoc = cfoc0
        if cang is None: cang = cang0

        a5plt.still(wallmesh, points=points, data=data, log=log, cpos=cpos,
                    cfoc=cfoc, cang=cang, axes=axes, cax=cax)


    def plotwall_3dinteractive(self, wallmesh=None, *args, points=None,
                               data=None, log=False, cpos=None, cfoc=None,
                               cang=None):
        """
        Open vtk window to display interactive view of the wall mesh.

        Args:
            wallmesh, optional : Polydata <br>
                Mesh representing the wall. If not given then construct the
                mesh from the wall data.
            *args : tuple (str, method), optional <br>
                Key (str) method pairs. When key is pressed when the plot is
                displayed, the associated method is called. The method should
                take Plotter instance as an argument.
            points : array_like, optional <br>
                Array Npoint x 3 defining points (markers) to be shown. For
                each point [x, y, z] coordinates are given. If boolean True is
                given, then markers are read from the endstate.
            cpos : array_like, optional <br>
                Camera position coordinates [x, y, z].
            cfoc : array_like, optional <br>
                Camera focal point coordinates [x, y, z].
            cang : array_like, optional <br>
                Camera angle [azimuth, elevation, roll].
        """
        if wallmesh is None:
            wallmesh = self.getwall_3dmesh()
        if isinstance(points, bool) and points == True:
            points = self.getstate_pointcloud(endcond="wall")

        (cpos0, cfoc0, cang0) = a5plt.defaultcamera(wallmesh)
        if cpos is None: cpos = cpos0
        if cfoc is None: cfoc = cfoc0
        if cang is None: cang = cang0

        a5plt.interactive(wallmesh, *args, points=points, data=data, log=log,
                          cpos=cpos, cfoc=cfoc, cang=cang)
