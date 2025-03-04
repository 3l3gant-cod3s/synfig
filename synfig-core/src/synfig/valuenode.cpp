/* === S Y N F I G ========================================================= */
/*!	\file valuenode.cpp
**	\brief Valuenodes
**
**	\legal
**	Copyright (c) 2002-2005 Robert B. Quattlebaum Jr., Adrian Bentley
**	Copyright (c) 2007, 2008 Chris Moore
**	Copyright (c) 2008, 2011 Carlos López
**	Copyright (c) 2016 caryoscelus
**
**	This file is part of Synfig.
**
**	Synfig is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 2 of the License, or
**	(at your option) any later version.
**
**	Synfig is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with Synfig.  If not, see <https://www.gnu.org/licenses/>.
**	\endlegal
*/
/* ========================================================================= */

/* === H E A D E R S ======================================================= */

#ifdef USING_PCH
#	include "pch.h"
#else
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "valuenode.h"
#include "valuenode_registry.h"
#include "general.h"
#include <synfig/localization.h>
#include "canvas.h"
#include "layer.h"
#include <algorithm>

#endif

/* === U S I N G =========================================================== */

using namespace synfig;

/* === M A C R O S ========================================================= */

/* === G L O B A L S ======================================================= */

static int value_node_count(0);

/* === P R O C E D U R E S ================================================= */

ValueNode::LooseHandle
synfig::find_value_node(const GUID& guid)
{
	return guid_cast<ValueNode>(guid);
}

/* === M E T H O D S ======================================================= */

void
ValueNode::breakpoint()
{
	return;
}

ValueNode::ValueNode(Type &type):type(&type)
{
	value_node_count++;
}

bool
LinkableValueNode::set_link(int i,ValueNode::Handle x)
{
	ValueNode::Handle previous(get_link(i));

	if(set_link_vfunc(i,x))
	{
		// Fix 2412072: remove the previous link from the parent_set unless one of the other links is also
		// using it when we convert a value to 'switch', both 'on' and 'off' are linked to the same valuenode
		// if we then disconnect one of the two, the one we disconnect is set to be a new valuenode_const
		// and the previously shared value is removed from the parent set even though the other is still
		// using it
		if(previous)
		{
			int size = link_count(), index;
			for (index=0; index < size; ++index)
			{
				if (i == index) continue;
				if (get_link(index) == previous)
					break;
			}
			if (index == size)
				remove_child(previous.get());
		}
		add_child(x.get());

		if(!x->is_exported() && get_parent_canvas())
		{
			x->set_parent_canvas(get_parent_canvas());
		}
		changed();
		return true;
	}
	return false;
}

ValueNode::LooseHandle
LinkableValueNode::get_link(int i)const
{
	return get_link_vfunc(i);
}

void
LinkableValueNode::unlink_all()
{
	for(int i=0;i<link_count();i++)
	{
		ValueNode::LooseHandle value_node(get_link(i));
		if(value_node)
			remove_child(value_node.get());
	}
}

ValueNode::~ValueNode()
{
	value_node_count--;

	begin_delete();
}

void
ValueNode::on_changed()
{
	DEBUG_LOG("SYNFIG_DEBUG_ON_CHANGED",
		"%s:%d ValueNode::on_changed()\n", __FILE__, __LINE__);

	etl::loose_handle<Canvas> parent_canvas = get_parent_canvas();
	if(parent_canvas)
		do						// signal to all the ancestor canvases
			parent_canvas->signal_value_node_changed()(this);
		while ( (parent_canvas = parent_canvas->parent()) );
	else if(get_root_canvas())
		get_root_canvas()->signal_value_node_changed()(this);

	Node::on_changed();
}

int
ValueNode::replace(etl::handle<ValueNode> x)
{
	if(x.get()==this)
		return 0;

	while(parent_count())
	{
		get_first_parent()->add_child(x.get());
		get_first_parent()->remove_child(this);
		//x->parent_set.insert(*parent_set.begin());
		//parent_set.erase(parent_set.begin());
	}
	int r(RHandle(this).replace(x));
	x->changed();
	return r;
}

void
ValueNode::set_id(const String &x)
{
	if(name!=x)
	{
		name=x;
		signal_id_changed_();
	}
}

String
ValueNode::get_description(bool show_exported_name)const
{
	if (const LinkableValueNode* value_node = dynamic_cast<const LinkableValueNode*>(this))
		return value_node->get_description(-1, show_exported_name);

	String ret(_("ValueNode"));

	if (show_exported_name && !is_exported())
		show_exported_name = false;

	if (show_exported_name)
		ret += strprintf(" (%s)", get_id().c_str());

	return ret;
}

bool
ValueNode::is_ancestor_of(ValueNode::Handle value_node_dest) const
{
	if (this == value_node_dest)
		return true;
	if (!value_node_dest)
		return false;
	if (!value_node_dest->parent_count())
		return false;

	ValueNode::Handle value_node_parent;
	value_node_parent = value_node_dest->find_first_parent_of_type<ValueNode>([=](ValueNode::Handle node) {
		return is_ancestor_of(node);
	});

	return value_node_parent? true : false;
}

void
ValueNode::get_values(std::set<ValueBase> &x) const
{
	std::map<Time, ValueBase> v;
	get_values(v);
	for(std::map<Time, ValueBase>::const_iterator i = v.begin(); i != v.end(); ++i)
		x.insert(i->second);
}

void
ValueNode::get_value_change_times(std::set<Time> &x) const
{
	std::map<Time, ValueBase> v;
	get_values(v);
	for(std::map<Time, ValueBase>::const_iterator i = v.begin(); i != v.end(); ++i)
		x.insert(i->first);
}

void
ValueNode::get_values(std::map<Time, ValueBase> &x) const
	{ get_values_vfunc(x); }

int
ValueNode::time_to_frame(Time t, Real fps)
	{ return (int)floor(t*fps + 1e-10); }

int
ValueNode::time_to_frame(Time t)
{
	if (Canvas::Handle canvas = get_parent_canvas())
	{
		const RendDesc &desc = canvas->rend_desc();
		return time_to_frame(t, desc.get_frame_rate());
	}
	return 0;
}

void
ValueNode::add_value_to_map(std::map<Time, ValueBase> &x, Time t, const ValueBase &v)
{
	std::map<Time, ValueBase>::const_iterator j = x.upper_bound(t);
	if (j == x.begin() || (--j)->second != v)
		{ ValueBase tmp(v); x[t] = tmp; }
}

void
ValueNode::canvas_time_bounds(const Canvas &canvas, bool &found, Time &begin, Time &end, Real &fps)
{
	if (!found)
	{
		found = true;
		begin = canvas.rend_desc().get_time_start();
		end = canvas.rend_desc().get_time_end();
		fps = canvas.rend_desc().get_frame_rate();
	}
	else
	{
		begin = std::min(begin, canvas.rend_desc().get_time_start());
		end = std::min(end, canvas.rend_desc().get_time_end());
		fps = std::max(fps, (Real)canvas.rend_desc().get_frame_rate());
	}
}

void
ValueNode::find_time_bounds(const Node &node, bool &found, Time &begin, Time &end, Real &fps)
{
	auto find_func = [&found, &begin, &end, &fps] (const Node* node) -> bool
	{
		if (const Layer *layer = dynamic_cast<const Layer*>(node))
		{
			if (Canvas::Handle canvas = layer->get_canvas())
				canvas_time_bounds(*canvas->get_root(), found, begin, end, fps);
		}
		else
		{
			find_time_bounds(*node, found, begin, end, fps);
		}
		return false;
	};
	node.foreach_parent(find_func);
}

void
ValueNode::calc_time_bounds(int &begin, int &end, Real &fps) const
{
	bool found = false;
	Time b = 0.0;
	Time e = 10*60;
	fps = 24;

	find_time_bounds(*this, found, b, e, fps);
	if (get_parent_canvas())
		canvas_time_bounds(*get_parent_canvas(), found, b, e, fps);
	if (get_root_canvas())
		canvas_time_bounds(*get_root_canvas(), found, b, e, fps);

	begin = (int)floor(b*fps);
	end = (int)ceil(e*fps);
}

void
ValueNode::calc_values(std::map<Time, ValueBase> &x, int begin, int end, Real fps) const
{
	if (fabs(fps) > 1e-10)
	{
		Real k = 1.0/fps;
		if (begin > end) std::swap(begin, end);
		for(int i = begin; i <= end; ++i)
			add_value_to_map(x, i*k, (*this)(i*k));
	}
}

void
ValueNode::calc_values(std::map<Time, ValueBase> &x, int begin, int end) const
{
	int b, e;
	Real fps;
	calc_time_bounds(b, e, fps);
	calc_values(x, begin, end, fps);
}

void
ValueNode::calc_values(std::map<Time, ValueBase> &x) const
{
	int begin, end;
	Real fps;
	calc_time_bounds(begin, end, fps);
	calc_values(x, begin, end, fps);
}

void
ValueNode::get_values_vfunc(std::map<Time, ValueBase> &x) const
{
	calc_values(x);
}


ValueNodeList::ValueNodeList():
	placeholder_count_(0)
{
}

bool
ValueNodeList::count(const String &id)const
{
	const_iterator iter;

	if(id.empty())
		return false;

	for(iter=begin();iter!=end() && id!=(*iter)->get_id();++iter)
		;

	if(iter==end())
		return false;

	return true;
}

ValueNode::Handle
ValueNodeList::find(const String &id, bool might_fail)
{
	iterator iter;

	if(id.empty())
		throw Exception::IDNotFound("Empty ID");

	for(iter=begin();iter!=end() && id!=(*iter)->get_id();++iter)
		;

	if(iter==end())
	{
		if (!might_fail) ValueNode::breakpoint();
		throw Exception::IDNotFound("ValueNode in ValueNodeList: "+id);
	}

	return *iter;
}

ValueNode::ConstHandle
ValueNodeList::find(const String &id, bool might_fail)const
{
	const_iterator iter;

	if(id.empty())
		throw Exception::IDNotFound("Empty ID");

	for(iter=begin();iter!=end() && id!=(*iter)->get_id();++iter)
		;

	if(iter==end())
	{
		if (!might_fail) ValueNode::breakpoint();
		throw Exception::IDNotFound("ValueNode in ValueNodeList: "+id);
	}

	return *iter;
}

ValueNode::Handle
ValueNodeList::surefind(const String &id)
{
	if(id.empty())
		throw Exception::IDNotFound("Empty ID");

	ValueNode::Handle value_node;

	try
	{
		value_node=find(id, true);
	}
	catch(Exception::IDNotFound&)
	{
		value_node=PlaceholderValueNode::create();
		value_node->set_id(id);
		push_back(value_node);
		placeholder_count_++;
	}

	return value_node;
}

bool
ValueNodeList::erase(ValueNode::Handle value_node)
{
	assert(value_node);

	iterator iter;

	for(iter=begin();iter!=end();++iter)
		if(value_node.get()==iter->get())
		{
			std::list<ValueNode::RHandle>::erase(iter);
			if(PlaceholderValueNode::Handle::cast_dynamic(value_node))
				placeholder_count_--;
			return true;
		}
	return false;
}

bool
ValueNodeList::add(ValueNode::Handle value_node)
{
	if(!value_node)
		return false;
	if(value_node->get_id().empty())
		return false;

	try
	{
		ValueNode::RHandle other_value_node=find(value_node->get_id(), true);
		if(PlaceholderValueNode::Handle::cast_dynamic(other_value_node))
		{
			other_value_node->replace(value_node);
			placeholder_count_--;
			return true;
		}

		return false;
	}
	catch(Exception::IDNotFound&)
	{
		push_back(value_node);
		return true;
	}

	return false;
}

void
ValueNodeList::audit()
{
	iterator iter,next;

	for(next=begin(),iter=next++;iter!=end();iter=next++)
		if(iter->count()==1)
			std::list<ValueNode::RHandle>::erase(iter);
}


String
PlaceholderValueNode::get_name()const
{
	return "placeholder";
}

String
PlaceholderValueNode::get_local_name()const
{
	return _("Placeholder");
}

String
PlaceholderValueNode::get_string()const
{
	return String("PlaceholderValueNode: ") + get_guid().get_string();
}

ValueNode::Handle
PlaceholderValueNode::clone(Canvas::LooseHandle canvas, const GUID& deriv_guid)const
{
	ValueNode* ret(new PlaceholderValueNode());
	ret->set_guid(get_guid()^deriv_guid);
	ret->set_parent_canvas(canvas);
	return ret;
}

PlaceholderValueNode::Handle
PlaceholderValueNode::create(Type &type)
{
	DEBUG_LOG("SYNFIG_DEBUG_PLACEHOLDER_VALUENODE",
		"%s:%d PlaceholderValueNode::create\n", __FILE__, __LINE__);
	return new PlaceholderValueNode(type);
}

ValueBase
PlaceholderValueNode::operator()(Time /*t*/)const
{
	assert(0);
	return ValueBase();
}

PlaceholderValueNode::PlaceholderValueNode(Type &type):
	ValueNode(type)
{
}

ValueNode::Handle
LinkableValueNode::clone(Canvas::LooseHandle canvas, const GUID& deriv_guid)const
{
	{
		ValueNode* x(find_value_node(get_guid()^deriv_guid).get());
		if(x)
			return x;
	}

	int i;
	LinkableValueNode *ret=create_new();
	ret->set_guid(get_guid()^deriv_guid);

	for(i=0;i<link_count();i++)
	{
		ValueNode::Handle link=get_link_vfunc(i);
		if(!link->is_exported())
		{
			ValueNode::Handle value_node(find_value_node(link->get_guid()^deriv_guid));
			if(!value_node)
				value_node=link->clone(canvas, deriv_guid);
			ret->set_link(i,value_node);
		}
		else
			ret->set_link(i,link);
	}

	ret->set_parent_canvas(canvas);
	return ret;
}

String
ValueNode::get_relative_id(etl::loose_handle<const Canvas> x)const
{
	assert(is_exported());
	assert(canvas_);

	if(x.get()==canvas_.get())
		return get_id();

	return canvas_->_get_relative_id(x)+':'+get_id();
}

etl::loose_handle<Canvas>
ValueNode::get_parent_canvas()const
{
	DEBUG_LOG("SYNFIG_DEBUG_GET_PARENT_CANVAS",
		"%s:%d get_parent_canvas of %p is %p\n", __FILE__, __LINE__, this, canvas_.get());

	return canvas_;
}

etl::loose_handle<Canvas>
ValueNode::get_root_canvas()const
{
	DEBUG_LOG("SYNFIG_DEBUG_GET_PARENT_CANVAS",
		"%s:%d get_root_canvas of %p is %p\n", __FILE__, __LINE__, this, root_canvas_.get());

	return root_canvas_;
}

etl::loose_handle<Canvas>
ValueNode::get_non_inline_ancestor_canvas()const
{
	etl::loose_handle<Canvas> parent(get_parent_canvas());

	if (parent)
	{
		etl::loose_handle<Canvas> ret(parent->get_non_inline_ancestor());

		DEBUG_LOG("SYNFIG_DEBUG_GET_PARENT_CANVAS",
			"%s:%d get_non_inline_ancestor_canvas of %p is %p\n", __FILE__, __LINE__, this, ret.get());

		return ret;
	}

	return parent;
}

void
ValueNode::set_parent_canvas(etl::loose_handle<Canvas> x)
{
	DEBUG_LOG("SYNFIG_DEBUG_SET_PARENT_CANVAS",
		"%s:%d set_parent_canvas of %p to %p\n", __FILE__, __LINE__, this, x.get());

	canvas_=x;

	DEBUG_LOG("SYNFIG_DEBUG_SET_PARENT_CANVAS",
		"%s:%d now %p\n", __FILE__, __LINE__, canvas_.get());

	if(x) set_root_canvas(x);
}

void
ValueNode::set_root_canvas(etl::loose_handle<Canvas> x)
{
	DEBUG_LOG("SYNFIG_DEBUG_SET_PARENT_CANVAS",
		"%s:%d set_root_canvas of %p to %p - ", __FILE__, __LINE__, this, x.get());

	root_canvas_=x->get_root();

	DEBUG_LOG("SYNFIG_DEBUG_SET_PARENT_CANVAS",
		"now %p\n", root_canvas_.get());
}

String
ValueNode::get_string()const
{
	return String("ValueNode: ") + get_description();
}

void LinkableValueNode::get_times_vfunc(Node::time_set &set) const
{
	ValueNode::LooseHandle	h;

	int size = link_count();

	//just add it to the set...
	for(int i=0; i < size; ++i)
	{
		h = get_link(i);

		if(h)
		{
			const Node::time_set &tset = h->get_times();
			set.insert(tset.begin(),tset.end());
		}
	}
}

String
LinkableValueNode::get_description(int index, bool show_exported_name)const
{
	String description;

	if (index == -1)
	{
		description = strprintf(" Linkable ValueNode (%s)", get_local_name().c_str());
		if (show_exported_name && is_exported())
			description += strprintf(" (%s)", get_id().c_str());
	}
	else
	{
		description = String(":") + link_local_name(index);

		if (show_exported_name)
		{
			ValueNode::LooseHandle link(get_link(index));
			if (link->is_exported())
				description += strprintf(" (%s)", link->get_id().c_str());
		}
	}

	const synfig::Node* node = this;
	LinkableValueNode::ConstHandle parent_linkable_vn;

	// walk up through the valuenodes trying to find the layer at the top
	while (node->parent_count() && !dynamic_cast<const Layer*>(node))
	{
		LinkableValueNode::ConstHandle linkable_value_node(dynamic_cast<const LinkableValueNode*>(node));
		if (linkable_value_node)
		{
			String link;
			int cnt = linkable_value_node->link_count();
			for (int i = 0; i < cnt; i++)
				if (linkable_value_node->get_link(i) == parent_linkable_vn)
				{
					link = String(":") + linkable_value_node->link_local_name(i);
					break;
				}

			description = linkable_value_node->get_local_name() + link + (parent_linkable_vn?">":"") + description;
		}
		node = node->get_first_parent();
		parent_linkable_vn = linkable_value_node;
	}

	Layer::ConstHandle parent_layer(dynamic_cast<const Layer*>(node));
	if(parent_layer)
	{
		String param;
		const Layer::DynamicParamList &dynamic_param_list(parent_layer->dynamic_param_list());
		// loop to find the parameter in the dynamic parameter list - this gives us its name
		for (Layer::DynamicParamList::const_iterator iter = dynamic_param_list.begin(); iter != dynamic_param_list.end(); iter++)
			if (iter->second == parent_linkable_vn)
				param = String(":") + parent_layer->get_param_local_name(iter->first);
		description = strprintf("(%s)%s>%s",
								parent_layer->get_non_empty_description().c_str(),
								param.c_str(),
								description.c_str());
	}

	return description;
}

String
LinkableValueNode::get_description(bool show_exported_name)const
{
	return get_description(-1, show_exported_name);
}

String
LinkableValueNode::link_name(int i)const
{
	const auto& vocab = children_vocab;
	Vocab::const_iterator iter(vocab.begin());
	int j=0;
	for(;iter!=vocab.end() && j<i; iter++, j++) {}
	return iter!=vocab.end()?iter->get_name():String();
}

String
LinkableValueNode::link_local_name(int i)const
{
	const auto& vocab = children_vocab;
	Vocab::const_iterator iter(vocab.begin());
	int j=0;
	for(;iter!=vocab.end() && j<i; iter++, j++) {}
	return iter!=vocab.end()?iter->get_local_name():String();
}

int
LinkableValueNode::get_link_index_from_name(const String &name)const
{
	const auto& vocab = children_vocab;
	Vocab::const_iterator iter(vocab.begin());
	int j=0;
	for(; iter!=vocab.end(); iter++, j++)
		if(iter->get_name()==name) return j;
	throw Exception::BadLinkName(name);
}

int
LinkableValueNode::link_count()const
{
	return children_vocab.size();
}

const LinkableValueNode::Vocab&
LinkableValueNode::get_children_vocab()const
{
	return children_vocab;
}

void
LinkableValueNode::set_children_vocab(const Vocab &newvocab)
{
	children_vocab.assign(newvocab.begin(),newvocab.end());
}

void
LinkableValueNode::init_children_vocab()
{
	set_children_vocab(get_children_vocab_vfunc());
}

void
LinkableValueNode::set_root_canvas(etl::loose_handle<Canvas> x)
{
	ValueNode::set_root_canvas(x);
	for(int i = 0; i < link_count(); ++i)
		get_link(i)->set_root_canvas(x);
}

LinkableValueNode::InvertibleStatus LinkableValueNode::is_invertible(const Time& /*t*/, const ValueBase& /*target_value*/, int* link_index) const
{
	if (link_index)
		*link_index = -1;
	return INVERSE_NOT_SUPPORTED;
}

ValueBase LinkableValueNode::get_inverse(const Time& /*t*/, const ValueBase& /*target_value*/) const
{
	return ValueBase();
}

void
LinkableValueNode::get_values_vfunc(std::map<Time, ValueBase> &x) const
{
	std::set<Time> times;
	for(int i = 0; i < link_count(); ++i)
		if (ValueNode::Handle link = get_link(i))
			link->get_value_change_times(times);
	for(std::set<Time>::const_iterator i = times.begin(); i != times.end(); ++i)
		add_value_to_map(x, *i, (*this)(*i));
}
