#ifndef __PExpr_H
#define __PExpr_H
/*
 * Copyright (c) 1998-2011 Stephen Williams <steve@icarus.com>
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

# include  <string>
# include  <vector>
# include  <valarray>
# include  "netlist.h"
# include  "verinum.h"
# include  "LineInfo.h"
# include  "pform_types.h"

class Design;
class Module;
class LexicalScope;
class NetNet;
class NetExpr;
class NetScope;

/*
 * The PExpr class hierarchy supports the description of
 * expressions. The parser can generate expression objects from the
 * source, possibly reducing things that it knows how to reduce.
 */

class PExpr : public LineInfo {

    public:
      enum width_mode_t { SIZED, EXPAND, LOSSLESS, UNSIZED };

        // Flag values that can be passed to elaborate_expr.
      static const unsigned NO_FLAGS     = 0x0;
      static const unsigned NEED_CONST   = 0x1;
      static const unsigned SYS_TASK_ARG = 0x2;

      PExpr();
      virtual ~PExpr();

      virtual void dump(ostream&) const;

        // This method tests whether the expression contains any identifiers
        // that have not been previously declared in the specified scope or
        // in any containing scope. Any such identifiers are added to the
        // specified scope as scalar nets of the specified type.
        //
        // This operation must be performed by the parser, to ensure that
        // subsequent declarations do not affect the decision to create an
        // implicit net.
      virtual void declare_implicit_nets(LexicalScope*scope, NetNet::Type type);

        // This method tests whether the expression contains any
        // references to automatically allocated variables.
      virtual bool has_aa_term(Design*des, NetScope*scope) const;

	// This method tests the type and width that the expression wants
	// to be. It should be called before elaborating an expression to
	// figure out the type and width of the expression. It also figures
	// out the minimum width that can be used to evaluate the expression
	// without changing the result. This allows the expression width to
	// be pruned when not all bits of the result are used.
	//
	// Normally mode should be initialised to SIZED before starting to
	// test the width of an expression. In SIZED mode the expression
	// width will be calculated strictly according to the IEEE standard
	// rules for expression width.
	// If the expression contains an unsized literal, mode will be
	// changed to LOSSLESS. In LOSSLESS mode the expression width will
	// be calculated as the minimum width necessary to avoid arithmetic
	// overflow or underflow.
	// If the expression both contains an unsized literal and contains
	// an operation that coerces a vector operand to a different type
	// (signed <-> unsigned), mode is changed to UNSIZED. UNSIZED mode
	// is the same as LOSSLESS, except that the final expression width
	// will be forced to be at least integer_width. This is necessary
	// to ensure compatibility with the IEEE standard, which requires
	// unsized literals to be treated as having the same width as an
	// integer. The lossless width calculation is inadequate in this
	// case because coercing an operand to a different type means that
	// the expression no longer obeys the normal rules of arithmetic.
	//
	// If mode is initialised to EXPAND instead of SIZED, the expression
	// width will be calculated as the minimum width necessary to avoid
	// arithmetic overflow or underflow, even if it contains no unsized
	// literals. mode will be changed LOSSLESS or UNSIZED as described
	// above. This supports a non-standard mode of expression width
	// calculation.
	//
	// When the final value of mode is UNSIZED, the width returned by
	// this method is the calculated lossless width, but the width
	// returned by a subsequent call to the expr_width method will be
	// the final expression width.
      virtual unsigned test_width(Design*des, NetScope*scope,
				  width_mode_t&mode);

	// After the test_width method is complete, these methods
	// return valid results.
      ivl_variable_type_t expr_type() const { return expr_type_; }
      unsigned expr_width() const           { return expr_width_; }
      unsigned min_width() const            { return min_width_; }
      bool has_sign() const                 { return signed_flag_; }

        // This method allows the expression type (signed/unsigned)
        // to be propagated down to any context-dependant operands.
      void cast_signed(bool flag) { signed_flag_ = flag; }

	// Procedural elaboration of the expression. The expr_width is
	// the required width of the expression.
	//
	// The sys_task_arg flag is true if expressions are allowed to
	// be incomplete.
      virtual NetExpr*elaborate_expr(Design*des, NetScope*scope,
				     unsigned expr_wid,
                                     unsigned flags) const;

	// This method elaborates the expression as gates, but
	// restricted for use as l-values of continuous assignments.
      virtual NetNet* elaborate_lnet(Design*des, NetScope*scope) const;

	// This is similar to elaborate_lnet, except that the
	// expression is evaluated to be bi-directional. This is
	// useful for arguments to inout ports of module instances and
	// ports of tran primitives.
      virtual NetNet* elaborate_bi_net(Design*des, NetScope*scope) const;

	// Expressions that can be in the l-value of procedural
	// assignments can be elaborated with this method. If the
	// is_force flag is true, then the set of valid l-value types
	// is slightly modified to accommodate the Verilog force
	// statement
      virtual NetAssign_* elaborate_lval(Design*des,
					 NetScope*scope,
					 bool is_force) const;

	// This attempts to evaluate a constant expression, and return
	// a verinum as a result. If the expression cannot be
	// evaluated, return 0.
      virtual verinum* eval_const(Design*des, NetScope*sc) const;

	// This method returns true if the expression represents a
        // structural net that can have multiple drivers. This is
        // used to test whether an input port connection can be
        // collapsed to a single wire.
      virtual bool is_collapsible_net(Design*des, NetScope*scope) const;

	// This method returns true if that expression is the same as
	// this expression. This method is used for comparing
	// expressions that must be structurally "identical".
      virtual bool is_the_same(const PExpr*that) const;

    protected:
      unsigned fix_width_(width_mode_t mode);

	// The derived class test_width methods should fill these in.
      ivl_variable_type_t expr_type_;
      unsigned expr_width_;
      unsigned min_width_;
      bool signed_flag_;

    private: // not implemented
      PExpr(const PExpr&);
      PExpr& operator= (const PExpr&);
};

ostream& operator << (ostream&, const PExpr&);

class PEConcat : public PExpr {

    public:
      PEConcat(const list<PExpr*>&p, PExpr*r =0);
      ~PEConcat();

      virtual verinum* eval_const(Design*des, NetScope*sc) const;
      virtual void dump(ostream&) const;

      virtual void declare_implicit_nets(LexicalScope*scope, NetNet::Type type);

      virtual bool has_aa_term(Design*des, NetScope*scope) const;

      virtual unsigned test_width(Design*des, NetScope*scope,
				  width_mode_t&mode);

      virtual NetNet* elaborate_lnet(Design*des, NetScope*scope) const;
      virtual NetNet* elaborate_bi_net(Design*des, NetScope*scope) const;
      virtual NetExpr*elaborate_expr(Design*des, NetScope*,
				     unsigned expr_wid,
                                     unsigned flags) const;
      virtual NetAssign_* elaborate_lval(Design*des,
					 NetScope*scope,
					 bool is_force) const;
      virtual bool is_collapsible_net(Design*des, NetScope*scope) const;
    private:
      NetNet* elaborate_lnet_common_(Design*des, NetScope*scope,
				     bool bidirectional_flag) const;
    private:
      vector<PExpr*>parms_;
      std::valarray<width_mode_t>width_modes_;

      PExpr*repeat_;
      NetScope*tested_scope_;
      unsigned repeat_count_;
};

/*
 * Event expressions are expressions that can be combined with the
 * event "or" operator. These include "posedge foo" and similar, and
 * also include named events. "edge" events are associated with an
 * expression, whereas named events simply have a name, which
 * represents an event variable.
 */
class PEEvent : public PExpr {

    public:
      enum edge_t {ANYEDGE, POSEDGE, NEGEDGE, POSITIVE};

	// Use this constructor to create events based on edges or levels.
      PEEvent(edge_t t, PExpr*e);

      ~PEEvent();

      edge_t type() const;
      PExpr* expr() const;

      virtual void dump(ostream&) const;

      virtual bool has_aa_term(Design*des, NetScope*scope) const;

    private:
      edge_t type_;
      PExpr *expr_;
};

/*
 * This holds a floating point constant in the source.
 */
class PEFNumber : public PExpr {

    public:
      explicit PEFNumber(verireal*vp);
      ~PEFNumber();

      const verireal& value() const;

	/* The eval_const method as applied to a floating point number
	   gets the *integer* value of the number. This accounts for
	   any rounding that is needed to get the value. */
      virtual verinum* eval_const(Design*des, NetScope*sc) const;

      virtual unsigned test_width(Design*des, NetScope*scope,
				  width_mode_t&mode);
      virtual NetExpr*elaborate_expr(Design*des, NetScope*,
				     unsigned expr_wid,
                                     unsigned flags) const;

      virtual void dump(ostream&) const;

    private:
      verireal*value_;
};

class PEIdent : public PExpr {

    public:
      explicit PEIdent(perm_string, bool no_implicit_sig=false);
      explicit PEIdent(const pform_name_t&);
      ~PEIdent();

	// Add another name to the string of hierarchy that is the
	// current identifier.
      void append_name(perm_string);

      virtual void dump(ostream&) const;

      virtual void declare_implicit_nets(LexicalScope*scope, NetNet::Type type);

      virtual bool has_aa_term(Design*des, NetScope*scope) const;

      virtual unsigned test_width(Design*des, NetScope*scope,
				  width_mode_t&mode);

	// Identifiers are allowed (with restrictions) is assign l-values.
      virtual NetNet* elaborate_lnet(Design*des, NetScope*scope) const;

      virtual NetNet* elaborate_bi_net(Design*des, NetScope*scope) const;

	// Identifiers are also allowed as procedural assignment l-values.
      virtual NetAssign_* elaborate_lval(Design*des,
					 NetScope*scope,
					 bool is_force) const;

      virtual NetExpr*elaborate_expr(Design*des, NetScope*,
				     unsigned expr_wid,
                                     unsigned flags) const;

	// Elaborate the PEIdent as a port to a module. This method
	// only applies to Ident expressions.
      NetNet* elaborate_port(Design*des, NetScope*sc) const;

      verinum* eval_const(Design*des, NetScope*sc) const;

      virtual bool is_collapsible_net(Design*des, NetScope*scope) const;

      const pform_name_t& path() const { return path_; }

    private:
      pform_name_t path_;
      bool no_implicit_sig_;

    private:
	// Common functions to calculate parts of part/bit
	// selects. These methods return true if the expressions
	// elaborate/calculate, or false if there is some sort of
	// source error.

	// The calculate_parts_ method calculates the range
	// expressions of a part select for the current object. The
	// part select expressions are elaborated and evaluated, and
	// the values written to the msb/lsb arguments. If there are
	// invalid bits (xz) in either expression, then the defined
	// flag is set to *false*.
      bool calculate_parts_(Design*, NetScope*, long&msb, long&lsb, bool&defined) const;
      NetExpr* calculate_up_do_base_(Design*, NetScope*, bool need_const) const;
      bool calculate_param_range_(Design*, NetScope*,
				  const NetExpr*msb_ex, long&msb,
				  const NetExpr*lsb_ex, long&lsb,
				  long length) const;

      bool calculate_up_do_width_(Design*, NetScope*, unsigned long&wid) const;

    private:
      NetAssign_*elaborate_lval_net_word_(Design*, NetScope*, NetNet*) const;
      bool elaborate_lval_net_bit_(Design*, NetScope*, NetAssign_*) const;
      bool elaborate_lval_net_part_(Design*, NetScope*, NetAssign_*) const;
      bool elaborate_lval_net_idx_(Design*, NetScope*, NetAssign_*,
                                   index_component_t::ctype_t) const;
      bool elaborate_lval_net_packed_member_(Design*, NetScope*,
					     NetAssign_*,
					     const perm_string&) const;

    private:
      NetExpr*elaborate_expr_param_(Design*des,
				    NetScope*scope,
				    const NetExpr*par,
				    NetScope*found_in,
				    const NetExpr*par_msb,
				    const NetExpr*par_lsb,
				    unsigned expr_wid,
                                    unsigned flags) const;
      NetExpr*elaborate_expr_param_part_(Design*des,
					 NetScope*scope,
					 const NetExpr*par,
					 NetScope*found_in,
					 const NetExpr*par_msb,
					 const NetExpr*par_lsb,
				         unsigned expr_wid) const;
      NetExpr*elaborate_expr_param_idx_up_(Design*des,
					   NetScope*scope,
					   const NetExpr*par,
					   NetScope*found_in,
					   const NetExpr*par_msb,
					   const NetExpr*par_lsb,
                                           bool need_const) const;
      NetExpr*elaborate_expr_param_idx_do_(Design*des,
					   NetScope*scope,
					   const NetExpr*par,
					   NetScope*found_in,
					   const NetExpr*par_msb,
					   const NetExpr*par_lsb,
                                           bool need_const) const;
      NetExpr*elaborate_expr_net(Design*des,
				 NetScope*scope,
				 NetNet*net,
				 NetScope*found,
				 unsigned expr_wid,
				 unsigned flags) const;
      NetExpr*elaborate_expr_net_word_(Design*des,
				       NetScope*scope,
				       NetNet*net,
				       NetScope*found,
				       unsigned expr_wid,
				       unsigned flags) const;
      NetExpr*elaborate_expr_net_part_(Design*des,
				       NetScope*scope,
				       NetESignal*net,
				       NetScope*found,
				       unsigned expr_wid) const;
      NetExpr*elaborate_expr_net_idx_up_(Design*des,
				         NetScope*scope,
				         NetESignal*net,
				         NetScope*found,
                                         bool need_const) const;
      NetExpr*elaborate_expr_net_idx_do_(Design*des,
				         NetScope*scope,
				         NetESignal*net,
				         NetScope*found,
                                         bool need_const) const;
      NetExpr*elaborate_expr_net_bit_(Design*des,
				      NetScope*scope,
				      NetESignal*net,
				      NetScope*found,
                                      bool need_const) const;

    private:
      NetNet* elaborate_lnet_common_(Design*des, NetScope*scope,
				     bool bidirectional_flag) const;

      bool eval_part_select_(Design*des, NetScope*scope, NetNet*sig,
			     long&midx, long&lidx) const;
};

class PENumber : public PExpr {

    public:
      explicit PENumber(verinum*vp);
      ~PENumber();

      const verinum& value() const;

      virtual void dump(ostream&) const;
      virtual unsigned test_width(Design*des, NetScope*scope,
				  width_mode_t&mode);

      virtual NetEConst*elaborate_expr(Design*des, NetScope*,
				       unsigned expr_wid, unsigned) const;
      virtual NetAssign_* elaborate_lval(Design*des,
					 NetScope*scope,
					 bool is_force) const;

      virtual verinum* eval_const(Design*des, NetScope*sc) const;

      virtual bool is_the_same(const PExpr*that) const;

    private:
      verinum*const value_;
};

/*
 * This represents a string constant in an expression.
 *
 * The s parameter to the PEString constructor is a C string that this
 * class instance will take for its own. The caller should not delete
 * the string, the destructor will do it.
 */
class PEString : public PExpr {

    public:
      explicit PEString(char*s);
      ~PEString();

      string value() const;
      virtual void dump(ostream&) const;

      virtual unsigned test_width(Design*des, NetScope*scope,
				  width_mode_t&mode);

      virtual NetEConst*elaborate_expr(Design*des, NetScope*,
				       unsigned expr_wid, unsigned) const;
      verinum* eval_const(Design*, NetScope*) const;

    private:
      char*text_;
};

class PEUnary : public PExpr {

    public:
      explicit PEUnary(char op, PExpr*ex);
      ~PEUnary();

      virtual void dump(ostream&out) const;

      virtual void declare_implicit_nets(LexicalScope*scope, NetNet::Type type);

      virtual bool has_aa_term(Design*des, NetScope*scope) const;

      virtual unsigned test_width(Design*des, NetScope*scope,
				  width_mode_t&mode);

      virtual NetExpr*elaborate_expr(Design*des, NetScope*,
				     unsigned expr_wid,
                                     unsigned flags) const;
      virtual verinum* eval_const(Design*des, NetScope*sc) const;

    private:
      NetExpr* elaborate_expr_bits_(NetExpr*operand, unsigned expr_wid) const;

    private:
      char op_;
      PExpr*expr_;
};

class PEBinary : public PExpr {

    public:
      explicit PEBinary(char op, PExpr*l, PExpr*r);
      ~PEBinary();

      virtual void dump(ostream&out) const;

      virtual void declare_implicit_nets(LexicalScope*scope, NetNet::Type type);

      virtual bool has_aa_term(Design*des, NetScope*scope) const;

      virtual unsigned test_width(Design*des, NetScope*scope,
				  width_mode_t&mode);

      virtual NetExpr*elaborate_expr(Design*des, NetScope*,
				     unsigned expr_wid,
                                     unsigned flags) const;
      virtual verinum* eval_const(Design*des, NetScope*sc) const;

    protected:
      char op_;
      PExpr*left_;
      PExpr*right_;

      NetExpr*elaborate_expr_base_(Design*, NetExpr*lp, NetExpr*rp,
				   unsigned expr_wid) const;
      NetExpr*elaborate_eval_expr_base_(Design*, NetExpr*lp, NetExpr*rp,
					unsigned expr_wid) const;

      NetExpr*elaborate_expr_base_bits_(Design*, NetExpr*lp, NetExpr*rp,
                                        unsigned expr_wid) const;
      NetExpr*elaborate_expr_base_div_(Design*, NetExpr*lp, NetExpr*rp,
				       unsigned expr_wid) const;
      NetExpr*elaborate_expr_base_mult_(Design*, NetExpr*lp, NetExpr*rp,
					unsigned expr_wid) const;
      NetExpr*elaborate_expr_base_add_(Design*, NetExpr*lp, NetExpr*rp,
				       unsigned expr_wid) const;

};

/*
 * Here are a few specialized classes for handling specific binary
 * operators.
 */
class PEBComp  : public PEBinary {

    public:
      explicit PEBComp(char op, PExpr*l, PExpr*r);
      ~PEBComp();

      virtual unsigned test_width(Design*des, NetScope*scope,
				  width_mode_t&mode);

      NetExpr* elaborate_expr(Design*des, NetScope*scope,
			      unsigned expr_wid, unsigned flags) const;

    private:
      unsigned l_width_;
      unsigned r_width_;
};

/*
 * This derived class is for handling logical expressions: && and ||.
*/
class PEBLogic  : public PEBinary {

    public:
      explicit PEBLogic(char op, PExpr*l, PExpr*r);
      ~PEBLogic();

      virtual unsigned test_width(Design*des, NetScope*scope,
				  width_mode_t&mode);

      NetExpr* elaborate_expr(Design*des, NetScope*scope,
			      unsigned expr_wid, unsigned flags) const;
};

/*
 * A couple of the binary operands have a special sub-expression rule
 * where the expression width is carried entirely by the left
 * expression, and the right operand is self-determined.
 */
class PEBLeftWidth  : public PEBinary {

    public:
      explicit PEBLeftWidth(char op, PExpr*l, PExpr*r);
      ~PEBLeftWidth() =0;

      virtual NetExpr*elaborate_expr_leaf(Design*des, NetExpr*lp, NetExpr*rp,
					  unsigned expr_wid) const =0;

    protected:
      virtual unsigned test_width(Design*des, NetScope*scope,
				  width_mode_t&mode);

      virtual NetExpr*elaborate_expr(Design*des, NetScope*scope,
				     unsigned expr_wid,
                                     unsigned flags) const;
};

class PEBPower  : public PEBLeftWidth {

    public:
      explicit PEBPower(char op, PExpr*l, PExpr*r);
      ~PEBPower();

      NetExpr*elaborate_expr_leaf(Design*des, NetExpr*lp, NetExpr*rp,
				  unsigned expr_wid) const;
};

class PEBShift  : public PEBLeftWidth {

    public:
      explicit PEBShift(char op, PExpr*l, PExpr*r);
      ~PEBShift();

      NetExpr*elaborate_expr_leaf(Design*des, NetExpr*lp, NetExpr*rp,
				  unsigned expr_wid) const;
};

/*
 * This class supports the ternary (?:) operator. The operator takes
 * three expressions, the test, the true result and the false result.
 */
class PETernary : public PExpr {

    public:
      explicit PETernary(PExpr*e, PExpr*t, PExpr*f);
      ~PETernary();

      virtual void dump(ostream&out) const;

      virtual void declare_implicit_nets(LexicalScope*scope, NetNet::Type type);

      virtual bool has_aa_term(Design*des, NetScope*scope) const;

      virtual unsigned test_width(Design*des, NetScope*scope,
				  width_mode_t&mode);

      virtual NetExpr*elaborate_expr(Design*des, NetScope*,
		                     unsigned expr_wid,
                                     unsigned flags) const;
      virtual verinum* eval_const(Design*des, NetScope*sc) const;

    private:
      NetExpr* elab_and_eval_alternative_(Design*des, NetScope*scope,
					  PExpr*expr, unsigned expr_wid,
                                          unsigned flags) const;

    private:
      PExpr*expr_;
      PExpr*tru_;
      PExpr*fal_;
};

/*
 * This class represents a parsed call to a function, including calls
 * to system functions. The parameters in the parms list are the
 * expressions that are passed as input to the ports of the function.
 */
class PECallFunction : public PExpr {
    public:
      explicit PECallFunction(const pform_name_t&n, const vector<PExpr *> &parms);
	// Call of system function (name is not hierarchical)
      explicit PECallFunction(perm_string n, const vector<PExpr *> &parms);
      explicit PECallFunction(perm_string n);

	// svector versions. Should be removed!
      explicit PECallFunction(const pform_name_t&n, const list<PExpr *> &parms);
      explicit PECallFunction(perm_string n, const list<PExpr *> &parms);

      ~PECallFunction();

      virtual void dump(ostream &) const;

      virtual void declare_implicit_nets(LexicalScope*scope, NetNet::Type type);

      virtual bool has_aa_term(Design*des, NetScope*scope) const;

      virtual NetExpr*elaborate_expr(Design*des, NetScope*scope,
				     unsigned expr_wid,
                                     unsigned flags) const;

      virtual unsigned test_width(Design*des, NetScope*scope,
				  width_mode_t&mode);

    private:
      pform_name_t path_;
      vector<PExpr *> parms_;

      bool check_call_matches_definition_(Design*des, NetScope*dscope) const;

      NetExpr* cast_to_width_(NetExpr*expr, unsigned wid) const;

      NetExpr* elaborate_sfunc_(Design*des, NetScope*scope,
                                unsigned expr_wid,
                                unsigned flags) const;
      NetExpr* elaborate_access_func_(Design*des, NetScope*scope, ivl_nature_t,
                                      unsigned expr_wid) const;
      unsigned test_width_sfunc_(Design*des, NetScope*scope,
			         width_mode_t&mode);
};

/*
 * Support the SystemVerilog cast to size.
 */
class PECastSize  : public PExpr {

    public:
      explicit PECastSize(unsigned expr_wid, PExpr*base);
      ~PECastSize();

      void dump(ostream &out) const;

      virtual NetExpr*elaborate_expr(Design*des, NetScope*scope,
				     unsigned expr_wid,
                                     unsigned flags) const;

      virtual unsigned test_width(Design*des, NetScope*scope,
				  width_mode_t&mode);

    private:
      unsigned size_;
      PExpr* base_;
};

/*
 * This class is used for error recovery. All methods do nothing and return
 * null or default values.
 */
class PEVoid : public PExpr {

    public:
      explicit PEVoid();
      ~PEVoid();

      virtual NetExpr*elaborate_expr(Design*des, NetScope*scope,
				     unsigned expr_wid,
                                     unsigned flags) const;
};

#endif
