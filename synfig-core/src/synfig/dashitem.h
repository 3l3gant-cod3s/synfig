/* === S Y N F I G ========================================================= */
/*!	\file dashitem.h
**	\brief Template Header for the implementation of a Dash Item
**
**	$Id$
**
**	\legal
**	Copyright (c) 2002-2005 Robert B. Quattlebaum Jr., Adrian Bentley
**	Copyright (c) 2011 Carlos López
**
**	This package is free software; you can redistribute it and/or
**	modify it under the terms of the GNU General Public License as
**	published by the Free Software Foundation; either version 2 of
**	the License, or (at your option) any later version.
**
**	This package is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
**	General Public License for more details.
**	\endlegal
*/
/* ========================================================================= */

/* === S T A R T =========================================================== */

#ifndef __SYNFIG_DASHITEM_H
#define __SYNFIG_DASHITEM_H

/* === H E A D E R S ======================================================= */

#include "widthpoint.h"

/* === M A C R O S ========================================================= */

/* === T Y P E D E F S ===================================================== */

/* === C L A S S E S & S T R U C T S ======================================= */

namespace synfig {

class DashItem : public WidthPoint
{
public:

	DashItem();
	DashItem(const DashItem &ref);
	DashItem(Real position, Real length, int sidebefore=TYPE_FLAT,
		int sideafter=TYPE_FLAT);

	const Real& get_length()const;
	void set_length(Real x);
	const Real& get_offset()const;
	void set_offset(Real x);
}; // END of class DashItem

}; // END of namespace synfig

/* === E N D =============================================================== */

#endif
