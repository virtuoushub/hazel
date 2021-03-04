module MakeSignatures =
       (Success: {
          module Poly: {type t('z);};
          type t;
        }, Extra: {type t;}) => {
  module type S = {
    let move: (Action.t, Success.t, Extra.t) => ActionOutcome.t(Success.t);

    let perform:
      (Contexts.t, Action.t, Success.t, Extra.t) => ActionOutcome.t(Success.t);
    let perform_opseq:
      (Contexts.t, Action.t, Success.t, Extra.t) => ActionOutcome.t(Success.t);
    let perform_operand:
      (Contexts.t, Action.t, Success.Poly.t(ZTyp.zoperand), Extra.t) =>
      ActionOutcome.t(Success.t);
  };
};

module UnitExtra = {
  type t = unit;
};
module KindExtra = {
  type t = Kind.t;
};
module type Syn_S = {
  module Success: {
    module Poly: {
      [@deriving sexp]
      type t('z) = {
        zty: 'z,
        kind: Kind.t,
        u_gen: MetaVarGen.t,
      };
    };

    [@deriving sexp]
    type t = Poly.t(ZTyp.t);
  };

  let mk_text:
    (Contexts.t, MetaVarGen.t, int, string) => ActionOutcome.t(Success.t);

  include MakeSignatures(Success)(UnitExtra).S;
};

module type Ana_S = {
  module Success: {
    module Poly: {
      [@deriving sexp]
      type t('z) = {
        zty: 'z,
        u_gen: MetaVarGen.t,
      };
    };

    [@deriving sexp]
    type t = Poly.t(ZTyp.t);
  };

  include MakeSignatures(Success)(KindExtra).S;
};

let operator_of_shape = (os: Action.operator_shape): option(UHTyp.operator) =>
  switch (os) {
  | SArrow => Some(Arrow)
  | SComma => Some(Prod)
  | SVBar => Some(Sum)
  | SMinus
  | SPlus
  | STimes
  | SDivide
  | SAnd
  | SOr
  | SLessThan
  | SGreaterThan
  | SEquals
  | SSpace
  | SCons => None
  };

let shape_of_operator = (op: UHTyp.operator): Action.operator_shape =>
  switch (op) {
  | Arrow => SArrow
  | Prod => SComma
  | Sum => SVBar
  };

let text_operand =
    (ctx: Contexts.t, u_gen: MetaVarGen.t, shape: TyTextShape.t)
    : (UHTyp.operand, MetaVarGen.t) =>
  switch (shape) {
  | Int => (Int, u_gen)
  | Bool => (Bool, u_gen)
  | Float => (Float, u_gen)
  | ExpandingKeyword(k) =>
    let (u, u_gen) = u_gen |> MetaVarGen.next;
    (
      TyVar(
        InVarHole(Keyword(k), u),
        k |> ExpandingKeyword.to_string |> TyId.of_string,
      ),
      u_gen,
    );
  | TyVar(id) =>
    if (TyVarCtx.contains(Contexts.tyvars(ctx), id)) {
      (TyVar(NotInVarHole, id), u_gen);
    } else {
      let (u, u_gen) = u_gen |> MetaVarGen.next;
      (TyVar(InVarHole(Free, u), id), u_gen);
    }
  };

let construct_operator =
    (
      operator: UHTyp.operator,
      zoperand: ZTyp.zoperand,
      (prefix, suffix): ZTyp.operand_surround,
    )
    : ZTyp.zopseq => {
  let operand = zoperand |> ZTyp.erase_zoperand;
  let (zoperand, surround) =
    if (ZTyp.is_before_zoperand(zoperand)) {
      let zoperand = UHTyp.Hole |> ZTyp.place_before_operand;
      let new_suffix = Seq.A(operator, S(operand, suffix));
      (zoperand, (prefix, new_suffix));
    } else {
      let zoperand = UHTyp.Hole |> ZTyp.place_before_operand;
      let new_prefix = Seq.A(operator, S(operand, prefix));
      (zoperand, (new_prefix, suffix));
    };
  ZTyp.mk_ZOpSeq(ZOperand(zoperand, surround));
};

module rec Syn: Syn_S = {
  module Success = {
    module Poly = {
      [@deriving sexp]
      type t('z) = {
        zty: 'z,
        kind: Kind.t,
        u_gen: MetaVarGen.t,
      };
    };

    [@deriving sexp]
    type t = Poly.t(ZTyp.t);

    let mk_result =
        (ctx: Contexts.t, u_gen: MetaVarGen.t, zty: ZTyp.t)
        : ActionOutcome.t(t) => {
      let hty = UHTyp.expand(Contexts.tyvars(ctx), zty |> ZTyp.erase);
      switch (Statics_Typ.syn(ctx, hty)) {
      | None => Failed
      | Some(kind) => Succeeded({zty, kind, u_gen})
      };
    };
  };
  open Success.Poly;

  let mk_text =
      (ctx: Contexts.t, u_gen: MetaVarGen.t, caret_index: int, text: string)
      : ActionOutcome.t(Success.t) => {
    let text_cursor = CursorPosition.OnText(caret_index);
    switch (TyTextShape.of_tyid(TyId.of_string(text))) {
    | None =>
      if (text |> StringUtil.is_empty) {
        Succeeded({
          zty: ZOpSeq.wrap(ZTyp.CursorT(OnDelim(0, Before), Hole)),
          kind: Kind.KHole,
          u_gen,
        });
      } else {
        Failed;
      }
    | Some(Bool) =>
      Succeeded({
        zty: ZOpSeq.wrap(ZTyp.CursorT(text_cursor, Bool)),
        kind: Kind.Type,
        u_gen,
      })
    | Some(Int) =>
      Succeeded({
        zty: ZOpSeq.wrap(ZTyp.CursorT(text_cursor, Int)),
        kind: Kind.Type,
        u_gen,
      })
    | Some(Float) =>
      Succeeded({
        zty: ZOpSeq.wrap(ZTyp.CursorT(text_cursor, Float)),
        kind: Kind.Type,
        u_gen,
      })
    | Some(ExpandingKeyword(k)) =>
      let (u, u_gen) = u_gen |> MetaVarGen.next;
      Succeeded({
        zty:
          ZOpSeq.wrap(
            ZTyp.CursorT(
              text_cursor,
              TyVar(
                InVarHole(Keyword(k), u),
                k |> ExpandingKeyword.to_string |> TyId.of_string,
              ),
            ),
          ),
        kind: Kind.KHole,
        u_gen,
      });
    | Some(TyVar(x)) =>
      if (TyVarCtx.contains(Contexts.tyvars(ctx), x)) {
        let idx = TyVarCtx.index_of_exn(Contexts.tyvars(ctx), x);
        let (_, k) = TyVarCtx.tyvar_with_idx(Contexts.tyvars(ctx), idx);
        Succeeded({
          zty:
            ZOpSeq.wrap(ZTyp.CursorT(text_cursor, TyVar(NotInVarHole, x))),
          kind: k,
          u_gen,
        });
      } else {
        let (u, u_gen) = u_gen |> MetaVarGen.next;
        Succeeded({
          zty:
            ZOpSeq.wrap(
              ZTyp.CursorT(text_cursor, TyVar(InVarHole(Free, u), x)),
            ),
          kind: Kind.KHole,
          u_gen,
        });
      }
    };
  };

  let rec move =
          (
            a: Action.t,
            {zty, kind: _, u_gen: _} as syn_r: Success.t,
            (): unit,
          )
          : ActionOutcome.t(Success.t) =>
    switch (a) {
    | MoveTo(path) =>
      switch (CursorPath_Typ.follow(path, zty |> ZTyp.erase)) {
      | None => Failed
      | Some(zty) => Succeeded({...syn_r, zty})
      }
    | MoveToPrevHole =>
      switch (
        CursorPath_common.(prev_hole_steps(CursorPath_Typ.holes_z(zty, [])))
      ) {
      | None => Failed
      | Some(steps) =>
        switch (CursorPath_Typ.of_steps(steps, zty |> ZTyp.erase)) {
        | None => Failed
        | Some(path) => move(MoveTo(path), syn_r, ())
        }
      }
    | MoveToNextHole =>
      switch (
        CursorPath_common.(next_hole_steps(CursorPath_Typ.holes_z(zty, [])))
      ) {
      | None => Failed
      | Some(steps) =>
        switch (CursorPath_Typ.of_steps(steps, zty |> ZTyp.erase)) {
        | None => Failed
        | Some(path) => move(MoveTo(path), syn_r, ())
        }
      }
    | MoveLeft =>
      switch (ZTyp.move_cursor_left(zty)) {
      | None => ActionOutcome.CursorEscaped(Before)
      | Some(z) => Succeeded({...syn_r, zty: z})
      }
    | MoveRight =>
      switch (ZTyp.move_cursor_right(zty)) {
      | None => ActionOutcome.CursorEscaped(After)
      | Some(z) => Succeeded({...syn_r, zty: z})
      }
    | Construct(_)
    | Delete
    | Backspace
    | UpdateApPalette(_)
    | SwapLeft
    | SwapRight
    | SwapUp
    | SwapDown
    | Init =>
      failwith(
        __LOC__
        ++ ": expected movement action, got "
        ++ Sexplib.Sexp.to_string(Action.sexp_of_t(a)),
      )
    };

  let insert_text = Action_common.syn_insert_text_(~mk_syn_text=mk_text);
  let backspace_text =
    Action_common.syn_backspace_text_(~mk_syn_text=mk_text);
  let delete_text = Action_common.syn_delete_text_(~mk_syn_text=mk_text);

  let split_text =
      (
        ctx: Contexts.t,
        u_gen: MetaVarGen.t,
        caret_index: int,
        sop: Action.operator_shape,
        text: string,
      )
      : ActionOutcome.t(Success.t) => {
    let (l, r) = text |> StringUtil.split_string(caret_index);
    switch (
      TyTextShape.of_tyid(TyId.of_string(l)),
      operator_of_shape(sop),
      TyTextShape.of_tyid(TyId.of_string(r)),
    ) {
    | (None, _, _)
    | (_, None, _)
    | (_, _, None) => Failed
    | (Some(lshape), Some(op), Some(rshape)) =>
      let (loperand, u_gen) = text_operand(ctx, u_gen, lshape);
      let (roperand, u_gen) = text_operand(ctx, u_gen, rshape);
      let new_zty = {
        let zoperand = roperand |> ZTyp.place_before_operand;
        ZTyp.mk_ZOpSeq(ZOperand(zoperand, (A(op, S(loperand, E)), E)));
      };
      switch (
        Statics_Typ.syn(
          ctx,
          UHTyp.expand(Contexts.tyvars(ctx), ZTyp.erase(new_zty)),
        )
      ) {
      | None => Failed
      | Some(kind) => Succeeded({zty: new_zty, kind, u_gen})
      };
    };
  };

  let rec perform =
          (ctx: Contexts.t, a: Action.t, syn_r: Success.t, (): unit)
          : ActionOutcome.t(Success.t) =>
    perform_opseq(ctx, a, syn_r, ())
  and perform_opseq =
      (
        ctx: Contexts.t,
        a: Action.t,
        {zty: ZOpSeq(skel, zseq) as zopseq, kind: _, u_gen} as syn_r: Success.t,
        (): unit,
      )
      : ActionOutcome.t(Success.t) =>
    switch (a, zseq) {
    /* Invalid actions at the type level */
    | (
        UpdateApPalette(_) |
        Construct(
          SAnn | SLet | SLine | SLam | SListNil | SInj(_) | SCase |
          SApPalette(_),
        ) |
        SwapUp |
        SwapDown,
        _,
      )
    /* Invalid cursor positions */
    | (_, ZOperator((OnText(_) | OnDelim(_), _), _)) => Failed

    /* Movement handled at top level */
    | (MoveTo(_) | MoveToPrevHole | MoveToNextHole | MoveLeft | MoveRight, _) =>
      move(a, syn_r, ())

    /* Deletion */

    | (Delete, ZOperator((OnOp(After as side), _), _))
    | (Backspace, ZOperator((OnOp(Before as side), _), _)) =>
      perform_opseq(ctx, Action_common.escape(side), syn_r, ())

    /* Delete before operator == Backspace after operator */
    | (Delete, ZOperator((OnOp(Before), op), surround)) =>
      perform_opseq(
        ctx,
        Backspace,
        {
          ...syn_r,
          zty: ZOpSeq(skel, ZOperator((OnOp(After), op), surround)),
        },
        (),
      )
    /* ... + [k-2] + [k-1] +<| [k] + ...   ==>   ... + [k-2] + [k-1]| + ...
     * (for now until we have proper type constructors) */
    | (Backspace, ZOperator((OnOp(After), _), (prefix, suffix))) =>
      let S(prefix_hd, new_prefix) = prefix;
      let zoperand = prefix_hd |> ZTyp.place_after_operand;
      let S(_, new_suffix) = suffix;
      Success.mk_result(
        ctx,
        u_gen,
        ZTyp.mk_ZOpSeq(ZOperand(zoperand, (new_prefix, new_suffix))),
      );

    /* Construction */
    /* construction on operators becomes movement... */
    | (Construct(SOp(SSpace)), ZOperator((OnOp(After), _), _)) =>
      perform_opseq(ctx, MoveRight, syn_r, ())
    /* ...or construction after movement */
    | (Construct(_) as a, ZOperator((OnOp(side), _), _)) =>
      switch (perform_opseq(ctx, Action_common.escape(side), syn_r, ())) {
      | Failed
      | CursorEscaped(_) => Failed
      | Succeeded(syn_r') => perform(ctx, a, syn_r', ())
      }

    /* Space becomes movement until we have proper type constructors */
    | (Construct(SOp(SSpace)), ZOperand(zoperand, _))
        when ZTyp.is_after_zoperand(zoperand) =>
      perform_opseq(ctx, MoveRight, syn_r, ())

    | (Construct(SOp(os)), ZOperand(CursorT(_) as zoperand, surround)) =>
      open ActionOutcome.Syntax;
      let* op = operator_of_shape(os) |> ActionOutcome.of_option;
      Success.mk_result(
        ctx,
        u_gen,
        construct_operator(op, zoperand, surround),
      );

    /* SwapLeft and SwapRight is handled at block level */

    | (SwapLeft, ZOperator(_))
    | (SwapRight, ZOperator(_)) => Failed

    | (SwapLeft, ZOperand(CursorT(_), (E, _))) => Failed
    | (
        SwapLeft,
        ZOperand(
          CursorT(_) as zoperand,
          (A(operator, S(operand, new_prefix)), suffix),
        ),
      ) =>
      let new_suffix = Seq.A(operator, S(operand, suffix));
      let new_zseq = ZSeq.ZOperand(zoperand, (new_prefix, new_suffix));
      Success.mk_result(ctx, u_gen, ZTyp.mk_ZOpSeq(new_zseq));
    | (SwapRight, ZOperand(CursorT(_), (_, E))) => Failed
    | (
        SwapRight,
        ZOperand(
          CursorT(_) as zoperand,
          (prefix, A(operator, S(operand, new_suffix))),
        ),
      ) =>
      let new_prefix = Seq.A(operator, S(operand, prefix));
      let new_zseq = ZSeq.ZOperand(zoperand, (new_prefix, new_suffix));
      Success.mk_result(ctx, u_gen, ZTyp.mk_ZOpSeq(new_zseq));

    /* Zipper */
    | (_, ZOperand(zoperand, (prefix, suffix))) =>
      let uhty = ZTyp.erase(ZOpSeq.wrap(zoperand));
      let hty = UHTyp.expand(Contexts.tyvars(ctx), uhty);
      open ActionOutcome.Syntax;
      let* kind = Statics_Typ.syn(ctx, hty) |> ActionOutcome.of_option;
      let* {zty: ZOpSeq(_, zseq), kind: _, u_gen} =
        perform_operand(ctx, a, {zty: zoperand, kind, u_gen}, ())
        |> ActionOutcome.rescue_escaped(side =>
             perform_opseq(
               ctx,
               Action_common.escape(side),
               {zty: zopseq, kind, u_gen},
               (),
             )
           );
      switch (zseq) {
      | ZOperand(zoperand, (inner_prefix, inner_suffix)) =>
        let new_prefix = Seq.affix_affix(inner_prefix, prefix);
        let new_suffix = Seq.affix_affix(inner_suffix, suffix);
        Success.mk_result(
          ctx,
          u_gen,
          ZTyp.mk_ZOpSeq(ZOperand(zoperand, (new_prefix, new_suffix))),
        );
      | ZOperator(zoperator, (inner_prefix, inner_suffix)) =>
        let new_prefix = Seq.seq_affix(inner_prefix, prefix);
        let new_suffix = Seq.seq_affix(inner_suffix, suffix);
        Success.mk_result(
          ctx,
          u_gen,
          ZTyp.mk_ZOpSeq(ZOperator(zoperator, (new_prefix, new_suffix))),
        );
      };
    | (Init, _) => failwith("Init action should not be performed.")
    }
  and perform_operand =
      (ctx: Contexts.t, a: Action.t, {zty: zoperand, kind, u_gen}, (): unit)
      : ActionOutcome.t(Success.t) =>
    switch (a, zoperand) {
    /* Invalid actions at the type level */
    | (
        UpdateApPalette(_) |
        Construct(
          SAnn | SLet | SLine | SLam | SListNil | SInj(_) | SCase |
          SApPalette(_) |
          SCommentLine,
        ) |
        SwapUp |
        SwapDown,
        _,
      ) =>
      Failed

    /* Movement handled at top level */
    | (MoveTo(_) | MoveToPrevHole | MoveToNextHole | MoveLeft | MoveRight, _) =>
      move(a, {zty: ZOpSeq.wrap(zoperand), kind, u_gen}, ())

    /* Backspace and Delete */

    | (
        Backspace,
        CursorT(OnText(caret_index), (Int | Bool | Float) as operand),
      ) =>
      backspace_text(ctx, u_gen, caret_index, operand |> UHTyp.to_string_exn)
    | (
        Delete,
        CursorT(OnText(caret_index), (Int | Bool | Float) as operand),
      ) =>
      delete_text(ctx, u_gen, caret_index, operand |> UHTyp.to_string_exn)

    /* ( _ <|)   ==>   ( _| ) */
    | (Backspace, CursorT(OnDelim(_, Before), _)) =>
      zoperand |> ZTyp.is_before_zoperand
        ? CursorEscaped(Before)
        : perform_operand(ctx, MoveLeft, {zty: zoperand, kind, u_gen}, ())
    /* (|> _ )   ==>   ( |_ ) */
    | (Delete, CursorT(OnDelim(_, After), _)) =>
      zoperand |> ZTyp.is_after_zoperand
        ? CursorEscaped(After)
        : perform_operand(ctx, MoveRight, {zty: zoperand, kind, u_gen}, ())

    /* Delete before delimiter == Backspace after delimiter */
    | (Delete, CursorT(OnDelim(k, Before), operand)) =>
      perform_operand(
        ctx,
        Backspace,
        {zty: CursorT(OnDelim(k, After), operand), kind, u_gen},
        (),
      )

    | (Backspace, CursorT(OnDelim(_, After), Hole | Unit)) =>
      Success.mk_result(
        ctx,
        u_gen,
        ZOpSeq.wrap(ZTyp.place_before_operand(Hole)),
      )

    | (
        Backspace,
        CursorT(OnDelim(_, After), Int | Float | Bool | TyVar(_, _)),
      ) =>
      failwith("Impossible: Int|Float|Bool|TyVar are treated as text")
    /* TyVar-related Backspace & Delete */
    | (Delete, CursorT(OnText(caret_index), TyVar(_, text))) =>
      delete_text(ctx, u_gen, caret_index, text |> TyId.to_string)
    | (Backspace, CursorT(OnText(caret_index), TyVar(_, text))) =>
      backspace_text(ctx, u_gen, caret_index, text |> TyId.to_string)

    /* ( _ )<|  ==>  _| */
    /* (<| _ )  ==>  |_ */
    | (
        Backspace,
        CursorT(OnDelim(k, After), Parenthesized(body) | List(body)),
      ) =>
      let place_cursor = k == 0 ? ZTyp.place_before : ZTyp.place_after;
      Success.mk_result(ctx, u_gen, body |> place_cursor);

    /* Construction */

    | (Construct(SOp(SSpace)), CursorT(OnDelim(_, After), _)) =>
      perform_operand(ctx, MoveRight, {zty: zoperand, kind, u_gen}, ())
    | (Construct(_) as a, CursorT(OnDelim(_, side), _))
        when
          !ZTyp.is_before_zoperand(zoperand)
          && !ZTyp.is_after_zoperand(zoperand) =>
      switch (
        perform_operand(
          ctx,
          Action_common.escape(side),
          {zty: zoperand, kind, u_gen},
          (),
        )
      ) {
      | (Failed | CursorEscaped(_)) as err => err
      | Succeeded(syn_r) => perform(ctx, a, syn_r, ())
      }

    | (Construct(SChar(s)), CursorT(_, Hole)) =>
      insert_text(ctx, u_gen, (0, s), "")
    | (
        Construct(SChar(s)),
        CursorT(OnText(j), (Int | Bool | Float) as operand),
      ) =>
      insert_text(ctx, u_gen, (j, s), operand |> UHTyp.to_string_exn)
    | (Construct(SChar(s)), CursorT(OnText(j), TyVar(_, x))) =>
      insert_text(ctx, u_gen, (j, s), x |> TyId.to_string)
    | (Construct(SChar(_)), CursorT(_)) => Failed

    | (Construct(SList), CursorT(_)) =>
      Success.mk_result(
        ctx,
        u_gen,
        ZOpSeq.wrap(ZTyp.ListZ(ZOpSeq.wrap(zoperand))),
      )

    | (Construct(SParenthesized), CursorT(_)) =>
      Success.mk_result(
        ctx,
        u_gen,
        ZOpSeq.wrap(ZTyp.ParenthesizedZ(ZOpSeq.wrap(zoperand))),
      )

    /* split */
    | (
        Construct(SOp(os)),
        CursorT(OnText(j), (Int | Bool | Float) as operand),
      )
        when
          !ZTyp.is_before_zoperand(zoperand)
          && !ZTyp.is_after_zoperand(zoperand) =>
      split_text(ctx, u_gen, j, os, operand |> UHTyp.to_string_exn)
    | (Construct(SOp(os)), CursorT(OnText(j), TyVar(_, id)))
        when
          !ZTyp.is_before_zoperand(zoperand)
          && !ZTyp.is_after_zoperand(zoperand) =>
      split_text(ctx, u_gen, j, os, id |> TyId.to_string)

    | (Construct(SOp(os)), CursorT(_)) =>
      open ActionOutcome.Syntax;
      let* op = operator_of_shape(os) |> ActionOutcome.of_option;
      Success.mk_result(
        ctx,
        u_gen,
        construct_operator(op, zoperand, (E, E)),
      );

    /* Invalid SwapLeft and SwapRight actions */
    | (SwapLeft | SwapRight, CursorT(_)) => Failed

    /* Zipper Cases */
    | (_, ParenthesizedZ(zbody)) =>
      open ActionOutcome.Syntax;
      let* {zty: zbody, u_gen, kind: _} =
        perform(ctx, a, {zty: zbody, kind, u_gen}, ())
        |> ActionOutcome.rescue_escaped(side =>
             perform_operand(
               ctx,
               Action_common.escape(side),
               {zty: zoperand, kind, u_gen},
               (),
             )
           );
      Success.mk_result(
        ctx,
        u_gen,
        ZOpSeq.wrap(ZTyp.ParenthesizedZ(zbody)),
      );
    | (_, ListZ(zbody)) =>
      open ActionOutcome.Syntax;
      let* {zty: zbody, kind: _, u_gen} =
        perform(ctx, a, {zty: zbody, kind, u_gen}, ())
        |> ActionOutcome.rescue_escaped(side =>
             perform_operand(
               ctx,
               Action_common.escape(side),
               {zty: zoperand, kind, u_gen},
               (),
             )
           );
      Success.mk_result(ctx, u_gen, ZOpSeq.wrap(ZTyp.ListZ(zbody)));

    /* Invalid cursor positions */
    | (_, CursorT(OnText(_) | OnOp(_), _)) => Failed
    | (_, CursorT(cursor, operand))
        when !ZTyp.is_valid_cursor_operand(cursor, operand) =>
      Failed

    | (Init, _) => failwith("Init action should not be performed.")
    };
}
and Ana: Ana_S = {
  module Success = {
    module Poly = {
      [@deriving sexp]
      type t('z) = {
        zty: 'z,
        u_gen: MetaVarGen.t,
      };

      let of_syn = ({Syn.Success.Poly.zty, kind: _, u_gen}) => {zty, u_gen};
    };

    [@deriving sexp]
    type t = Poly.t(ZTyp.t);

    let mk_result =
        (ctx: Contexts.t, u_gen: MetaVarGen.t, zty: ZTyp.t, k: Kind.t)
        : ActionOutcome.t(t) => {
      let hty = UHTyp.expand(Contexts.tyvars(ctx), zty |> ZTyp.erase);
      if (Statics_Typ.ana(ctx, hty, k)) {
        Succeeded({zty, u_gen});
      } else {
        Failed;
      };
    };
  };
  open Success.Poly;

  let rec move =
          (a: Action.t, {zty, u_gen: _} as ana_r, k: Kind.t)
          : ActionOutcome.t(Success.t) =>
    switch (a) {
    | MoveTo(path) =>
      switch (CursorPath_Typ.follow(path, zty |> ZTyp.erase)) {
      | None => Failed
      | Some(zty) => Succeeded({...ana_r, zty})
      }
    | MoveToPrevHole =>
      switch (
        CursorPath_common.(prev_hole_steps(CursorPath_Typ.holes_z(zty, [])))
      ) {
      | None => Failed
      | Some(steps) =>
        switch (CursorPath_Typ.of_steps(steps, zty |> ZTyp.erase)) {
        | None => Failed
        | Some(path) => move(MoveTo(path), ana_r, k)
        }
      }
    | MoveToNextHole =>
      switch (
        CursorPath_common.(next_hole_steps(CursorPath_Typ.holes_z(zty, [])))
      ) {
      | None => Failed
      | Some(steps) =>
        switch (CursorPath_Typ.of_steps(steps, zty |> ZTyp.erase)) {
        | None => Failed
        | Some(path) => move(MoveTo(path), ana_r, k)
        }
      }
    | MoveLeft =>
      zty
      |> ZTyp.move_cursor_left
      |> Option.map(z => ActionOutcome.Succeeded({...ana_r, zty: z}))
      |> OptUtil.get(() => ActionOutcome.CursorEscaped(Before))
    | MoveRight =>
      zty
      |> ZTyp.move_cursor_right
      |> Option.map(z => ActionOutcome.Succeeded({...ana_r, zty: z}))
      |> OptUtil.get(() => ActionOutcome.CursorEscaped(After))
    | Construct(_)
    | Delete
    | Backspace
    | UpdateApPalette(_)
    | SwapLeft
    | SwapRight
    | SwapUp
    | SwapDown
    | Init =>
      failwith(
        __LOC__
        ++ ": expected movement action, got "
        ++ Sexplib.Sexp.to_string(Action.sexp_of_t(a)),
      )
    };

  let rec perform = (ctx, a, s, k) => perform_opseq(ctx, a, s, k)
  and perform_opseq =
      (
        ctx: Contexts.t,
        a: Action.t,
        {zty: ZOpSeq(_, zseq) as zopseq, u_gen}: Success.t,
        k: Kind.t,
      )
      : ActionOutcome.t(Success.t) => {
    switch (a, zseq) {
    | (_, ZOperand(zoperand, (E, E))) =>
      perform_operand(ctx, a, {zty: zoperand, u_gen}, k)
    | (_, ZOperand(zoperand, (prefix, suffix))) =>
      open ActionOutcome.Syntax;
      let uhty = ZTyp.erase(ZOpSeq.wrap(zoperand));
      let hty = UHTyp.expand(Contexts.tyvars(ctx), uhty);
      let* kind = Statics_Typ.syn(ctx, hty) |> ActionOutcome.of_option;
      let* {zty: ZOpSeq(_, zseq), u_gen} =
        Syn.perform_operand(
          ctx,
          a,
          {Syn.Success.Poly.zty: zoperand, kind, u_gen},
          (),
        )
        |> Fn.flip(ActionOutcome.map, Success.Poly.of_syn)
        |> ActionOutcome.rescue_escaped(side => {
             perform_opseq(
               ctx,
               Action_common.escape(side),
               {zty: zopseq, u_gen},
               kind,
             )
           });
      switch (zseq) {
      | ZOperand(zoperand, (inner_prefix, inner_suffix)) =>
        let new_prefix = Seq.affix_affix(inner_prefix, prefix);
        let new_suffix = Seq.affix_affix(inner_suffix, suffix);
        Success.mk_result(
          ctx,
          u_gen,
          ZTyp.mk_ZOpSeq(ZOperand(zoperand, (new_prefix, new_suffix))),
          k,
        );
      | ZOperator(zoperator, (inner_prefix, inner_suffix)) =>
        let new_prefix = Seq.seq_affix(inner_prefix, prefix);
        let new_suffix = Seq.seq_affix(inner_suffix, suffix);
        Success.mk_result(
          ctx,
          u_gen,
          ZTyp.mk_ZOpSeq(ZOperator(zoperator, (new_prefix, new_suffix))),
          k,
        );
      };
    | (_, _) =>
      open ActionOutcome.Syntax;
      // It seems like in the type-variable-2 branch that every other case was exactly the same as the syn one

      let+ syn_r =
        Syn.perform_opseq(
          ctx,
          a,
          {Syn.Success.Poly.zty: zopseq, u_gen, kind: k},
          (),
        );
      Success.Poly.of_syn(syn_r);
    };
  }
  and perform_operand =
      (
        ctx: Contexts.t,
        a: Action.t,
        {zty: zoperand, u_gen}: Success.Poly.t(ZTyp.zoperand),
        kind: Kind.t,
      )
      : ActionOutcome.t(Success.t) => {
    open ActionOutcome.Syntax;

    // It seems like in the type-variable-2 branch that all of these cases are the same as syn
    let+ syn_r =
      Syn.perform_operand(
        ctx,
        a,
        {Syn.Success.Poly.zty: zoperand, kind, u_gen},
        (),
      );
    Success.Poly.of_syn(syn_r);
  };
};
