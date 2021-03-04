let operator_of_shape: Action.operator_shape => option(UHPat.binop) =
  fun
  | SComma => Some(Comma)
  | SSpace => Some(Space)
  | SCons => Some(Cons)
  | SAnd
  | SOr
  | SMinus
  | SPlus
  | STimes
  | SDivide
  | SLessThan
  | SGreaterThan
  | SEquals
  | SArrow
  | SVBar => None;

let shape_of_operator = (op: UHPat.binop): Action.operator_shape =>
  switch (op) {
  | Comma => SComma
  | Space => SSpace
  | Cons => SCons
  };

let shape_is_of_unop = (os: Action.operator_shape): bool =>
  switch (os) {
  | SMinus => true
  | _ => false
  };

let is_negative_literal = (lit: UHPat.operand): bool =>
  switch (lit) {
  | IntLit(_, n) => int_of_string(n) < 0
  | FloatLit(_, f) => float_of_string(f) < 0.0
  | _ => false
  };

let is_after_unop_of_negative_literal = (zoperand: ZPat.zoperand): bool =>
  switch (zoperand) {
  | CursorP(OnText(j), IntLit(_) as operand) =>
    is_negative_literal(operand) && j == 1
  | CursorP(OnText(j), FloatLit(_, f) as operand) =>
    is_negative_literal(operand) && (j == 1 || j == 2 && f.[1] == '.')
  | _ => false
  };

let syn_zunop_to_znumlit =
    (ctx: Contexts.t, u_gen: MetaVarGen.t, zunop: ZPat.zoperand)
    : option((ZPat.zoperand, HTyp.t, Contexts.t, MetaVarGen.t)) => {
  switch (zunop) {
  | UnaryOpZ(_, op, CursorP(OnText(j), IntLit(_, n) | FloatLit(_, n))) =>
    let (new_cursor, new_text): (CursorPosition.t, string) =
      switch (op) {
      | Negate => (OnText(j + 1), "-" ++ n)
      | FNegate => (OnText(j + 2), "-." ++ n)
      };
    switch (TextShape.of_text(new_text)) {
    | IntLit(n) =>
      Some((CursorP(new_cursor, IntLit(NotInHole, n)), Int, ctx, u_gen))
    | FloatLit(n) =>
      Some((CursorP(new_cursor, FloatLit(NotInHole, n)), Float, ctx, u_gen))
    | _ =>
      switch (Statics_Pat.syn_operand(ctx, ZPat.erase_zoperand(zunop))) {
      | Some((ty, ctx)) => Some((zunop, ty, ctx, u_gen))
      | None => None
      }
    };
  | _ =>
    switch (Statics_Pat.syn_operand(ctx, ZPat.erase_zoperand(zunop))) {
    | Some((ty, ctx)) => Some((zunop, ty, ctx, u_gen))
    | None => None
    }
  };
};

let ana_zunop_to_znumlit =
    (ctx: Contexts.t, u_gen: MetaVarGen.t, zunop: ZPat.zoperand, ty: HTyp.t)
    : option((ZPat.zoperand, Contexts.t, MetaVarGen.t)) => {
  switch (syn_zunop_to_znumlit(ctx, u_gen, zunop)) {
  | Some((zoperand, ty', ctx, u_gen)) =>
    if (HTyp.consistent(ty, ty')) {
      Some((zoperand, ctx, u_gen));
    } else {
      let (u, u_gen) = MetaVarGen.next(u_gen);
      let zoperand =
        zoperand |> ZPat.set_err_status_zoperand(InHole(TypeInconsistent, u));
      Some((zoperand, ctx, u_gen));
    }
  | None => None
  };
};

let znumlit_to_zunop = (znumlit: ZPat.zoperand): option(ZPat.zoperand) => {
  switch (znumlit) {
  | CursorP(_, lit) when !is_negative_literal(lit) => None
  | CursorP(OnText(0), IntLit(err, n)) =>
    Some(
      CursorP(
        OnText(0),
        UnaryOp(
          err,
          Negate,
          UHPat.intlit(String.sub(n, 1, String.length(n) - 1)),
        ),
      ),
    )
  | CursorP(OnText(0), FloatLit(err, f)) =>
    Some(
      CursorP(
        OnText(0),
        UnaryOp(
          err,
          FNegate,
          UHPat.floatlit(String.sub(f, 1, String.length(f) - 1)),
        ),
      ),
    )
  | CursorP(OnText(j), IntLit(err, n)) =>
    Some(
      UnaryOpZ(
        err,
        Negate,
        CursorP(
          OnText(j - 1),
          UHPat.intlit(String.sub(n, 1, String.length(n) - 1)),
        ),
      ),
    )
  | CursorP(OnText(j), FloatLit(err, f)) =>
    let op: Unops_Pat.t = Negate;
    let new_cursor = CursorPosition.OnText(j - 1);
    let operand = UHPat.floatlit(String.sub(f, 1, String.length(f) - 1));
    Some(UnaryOpZ(err, op, CursorP(new_cursor, operand)));
  | _ => None
  };
};

let has_Comma = (ZOpSeq(_, zseq): ZPat.zopseq) =>
  zseq
  |> ZPat.erase_zseq
  |> Seq.operators
  |> List.exists(op => op == Operators_Pat.Comma);

type syn_success = (ZPat.t, HTyp.t, Contexts.t, MetaVarGen.t);
type ana_success = (ZPat.t, Contexts.t, MetaVarGen.t);

let mk_and_syn_fix_OpSeq =
    (ctx: Contexts.t, u_gen: MetaVarGen.t, seq: UHPat.seq)
    : (UHPat.opseq, HTyp.t, Contexts.t, MetaVarGen.t) => {
  let opseq = UHPat.mk_OpSeq(seq);
  Statics_Pat.syn_fix_holes_opseq(ctx, u_gen, opseq);
};
let mk_and_ana_fix_OpSeq =
    (ctx: Contexts.t, u_gen: MetaVarGen.t, seq: UHPat.seq, ty: HTyp.t)
    : (UHPat.opseq, Contexts.t, MetaVarGen.t) => {
  let opseq = UHPat.mk_OpSeq(seq);
  Statics_Pat.ana_fix_holes_opseq(ctx, u_gen, opseq, ty);
};
let mk_and_syn_fix_ZOpSeq =
    (ctx: Contexts.t, u_gen: MetaVarGen.t, zseq: ZPat.zseq): syn_success => {
  let zopseq = ZPat.mk_ZOpSeq(zseq);
  Statics_Pat.syn_fix_holes_z(ctx, u_gen, zopseq);
};
let mk_and_ana_fix_ZOpSeq =
    (ctx: Contexts.t, u_gen: MetaVarGen.t, zseq: ZPat.zseq, ty: HTyp.t)
    : ana_success => {
  let zopseq = ZPat.mk_ZOpSeq(zseq);
  Statics_Pat.ana_fix_holes_z(ctx, u_gen, zopseq, ty);
};

let mk_syn_result =
    (ctx: Contexts.t, u_gen: MetaVarGen.t, zp: ZPat.t)
    : ActionOutcome.t(syn_success) =>
  switch (Statics_Pat.syn(ctx, zp |> ZPat.erase)) {
  | None => Failed
  | Some((ty, ctx)) => Succeeded((zp, ty, ctx, u_gen))
  };
let mk_ana_result =
    (ctx: Contexts.t, u_gen: MetaVarGen.t, zp: ZPat.t, ty: HTyp.t)
    : ActionOutcome.t(ana_success) =>
  switch (Statics_Pat.ana(ctx, zp |> ZPat.erase, ty)) {
  | None => Failed
  | Some(ctx) => Succeeded((zp, ctx, u_gen))
  };

let mk_syn_text =
    (ctx: Contexts.t, u_gen: MetaVarGen.t, caret_index: int, text: string)
    : ActionOutcome.t(syn_success) => {
  let text_cursor = CursorPosition.OnText(caret_index);
  switch (TextShape.of_text(text)) {
  | InvalidTextShape(t) =>
    if (text |> StringUtil.is_empty) {
      let (zhole, u_gen) = u_gen |> ZPat.new_EmptyHole;
      Succeeded((ZOpSeq.wrap(zhole), HTyp.Hole, ctx, u_gen));
    } else {
      let (it, u_gen) = UHPat.new_InvalidText(u_gen, t);
      let zp = ZOpSeq.wrap(ZPat.CursorP(text_cursor, it));
      Succeeded((zp, HTyp.Hole, ctx, u_gen));
    }
  | Underscore =>
    let zp = ZOpSeq.wrap(ZPat.CursorP(OnDelim(0, After), UHPat.wild()));
    Succeeded((zp, HTyp.Hole, ctx, u_gen));
  | IntLit(n) =>
    let zp = ZOpSeq.wrap(ZPat.CursorP(text_cursor, UHPat.intlit(n)));
    Succeeded((zp, HTyp.Int, ctx, u_gen));
  | FloatLit(f) =>
    let zp = ZOpSeq.wrap(ZPat.CursorP(text_cursor, UHPat.floatlit(f)));
    Succeeded((zp, HTyp.Float, ctx, u_gen));
  | BoolLit(b) =>
    let zp = ZOpSeq.wrap(ZPat.CursorP(text_cursor, UHPat.boollit(b)));
    Succeeded((zp, HTyp.Bool, ctx, u_gen));
  | ExpandingKeyword(k) =>
    let (u, u_gen) = u_gen |> MetaVarGen.next;
    let var =
      UHPat.var(
        ~var_err=InVarHole(Keyword(k), u),
        k |> ExpandingKeyword.to_string,
      );
    let zp = ZOpSeq.wrap(ZPat.CursorP(text_cursor, var));
    Succeeded((zp, HTyp.Hole, ctx, u_gen));
  | Var(x) =>
    let ctx = Contexts.extend_gamma(ctx, (x, Hole));
    let zp = ZOpSeq.wrap(ZPat.CursorP(text_cursor, UHPat.var(x)));
    Succeeded((zp, HTyp.Hole, ctx, u_gen));
  };
};

let mk_ana_text =
    (
      ctx: Contexts.t,
      u_gen: MetaVarGen.t,
      caret_index: int,
      text: string,
      ty: HTyp.t,
    )
    : ActionOutcome.t(ana_success) => {
  let text_cursor = CursorPosition.OnText(caret_index);
  switch (TextShape.of_text(text)) {
  | InvalidTextShape(t) =>
    if (text |> StringUtil.is_empty) {
      let (zhole, u_gen) = u_gen |> ZPat.new_EmptyHole;
      Succeeded((ZOpSeq.wrap(zhole), ctx, u_gen));
    } else {
      let (it, u_gen) = UHPat.new_InvalidText(u_gen, t);
      let zp = ZOpSeq.wrap(ZPat.CursorP(text_cursor, it));
      Succeeded((zp, ctx, u_gen));
    }
  | Underscore =>
    let zp = ZOpSeq.wrap(ZPat.CursorP(OnDelim(0, After), UHPat.wild()));
    Succeeded((zp, ctx, u_gen));
  | IntLit(_)
  | FloatLit(_)
  | BoolLit(_) =>
    switch (mk_syn_text(ctx, u_gen, caret_index, text)) {
    | (Failed | CursorEscaped(_)) as err => err
    | Succeeded((zp, ty', ctx, u_gen)) =>
      if (HTyp.consistent(ty, ty')) {
        Succeeded((zp, ctx, u_gen));
      } else {
        let (zp, u_gen) = zp |> ZPat.mk_inconsistent(u_gen);
        Succeeded((zp, ctx, u_gen));
      }
    }
  | ExpandingKeyword(k) =>
    let (u, u_gen) = u_gen |> MetaVarGen.next;
    let var = UHPat.var(~var_err=InVarHole(Keyword(k), u), text);
    let zp = ZOpSeq.wrap(ZPat.CursorP(text_cursor, var));
    Succeeded((zp, ctx, u_gen));
  | Var(x) =>
    let ctx = Contexts.extend_gamma(ctx, (x, ty));
    let zp = ZOpSeq.wrap(ZPat.CursorP(text_cursor, UHPat.var(x)));
    Succeeded((zp, ctx, u_gen));
  };
};

let syn_insert_text = Action_common.syn_insert_text_(~mk_syn_text);
let ana_insert_text = Action_common.ana_insert_text_(~mk_ana_text);
let syn_backspace_text = Action_common.syn_backspace_text_(~mk_syn_text);
let ana_backspace_text = Action_common.ana_backspace_text_(~mk_ana_text);
let syn_delete_text = Action_common.syn_delete_text_(~mk_syn_text);
let ana_delete_text = Action_common.ana_delete_text_(~mk_ana_text);

let syn_split_text =
    (
      ctx: Contexts.t,
      u_gen: MetaVarGen.t,
      caret_index: int,
      sop: Action.operator_shape,
      text: string,
    )
    : ActionOutcome.t(syn_success) => {
  let (l, r) = text |> StringUtil.split_string(caret_index);
  switch (
    TextShape.of_text(l),
    operator_of_shape(sop),
    TextShape.of_text(r),
  ) {
  | (_, None, _) => Failed
  | (lshape, Some(op), rshape) =>
    let (loperand, u_gen) = UHPat.text_operand(u_gen, lshape);
    let (roperand, u_gen) = UHPat.text_operand(u_gen, rshape);
    let new_zp = {
      let zoperand = roperand |> ZPat.place_before_operand;
      ZPat.mk_ZOpSeq(ZOperand(zoperand, (A(op, S(loperand, E)), E)));
    };
    Succeeded(Statics_Pat.syn_fix_holes_z(ctx, u_gen, new_zp));
  };
};
let ana_split_text =
    (
      ctx: Contexts.t,
      u_gen: MetaVarGen.t,
      caret_index: int,
      sop: Action.operator_shape,
      text: string,
      ty: HTyp.t,
    )
    : ActionOutcome.t(ana_success) => {
  let (l, r) = text |> StringUtil.split_string(caret_index);
  switch (
    TextShape.of_text(l),
    operator_of_shape(sop),
    TextShape.of_text(r),
  ) {
  | (_, None, _) => Failed
  | (lshape, Some(op), rshape) =>
    let (loperand, u_gen) = UHPat.text_operand(u_gen, lshape);
    let (roperand, u_gen) = UHPat.text_operand(u_gen, rshape);
    let new_zp = {
      let zoperand = roperand |> ZPat.place_before_operand;
      ZPat.mk_ZOpSeq(ZOperand(zoperand, (A(op, S(loperand, E)), E)));
    };
    Succeeded(Statics_Pat.ana_fix_holes_z(ctx, u_gen, new_zp, ty));
  };
};

let delete_operator =
  Action_common.delete_binop_(
    ~space=Operators_Pat.Space,
    ~is_EmptyHole=UHPat.is_EmptyHole,
    ~place_before_operand=ZPat.place_before_operand,
    ~place_after_operand=ZPat.place_after_operand,
    ~place_after_binop=ZPat.place_after_binop,
  );

let construct_operator_before_zoperand =
  Action_common.construct_operator_before_zoperand_(
    ~is_Space=Operators_Pat.is_Space,
    ~new_EmptyHole=UHPat.new_EmptyHole,
    ~erase_zoperand=ZPat.erase_zoperand,
    ~place_before_operand=ZPat.place_before_operand,
    ~place_after_binop=ZPat.place_after_binop,
  );
let construct_operator_after_zoperand =
  Action_common.construct_operator_after_zoperand_(
    ~is_Space=Operators_Pat.is_Space,
    ~new_EmptyHole=UHPat.new_EmptyHole,
    ~erase_zoperand=ZPat.erase_zoperand,
    ~place_before_operand=ZPat.place_before_operand,
    ~place_after_binop=ZPat.place_after_binop,
  );

let complete_tuple =
  Action_common.complete_tuple_(
    ~mk_ZOpSeq=ZPat.mk_ZOpSeq,
    ~comma=Operators_Pat.Comma,
    ~zcomma=(OnOp(After), Operators_Pat.Comma),
    ~new_EmptyHole=UHPat.new_EmptyHole,
  );

let resurround_z =
    (zp: ZPat.t, (prefix, suffix): ZPat.operand_surround): ZPat.zseq =>
  switch (zp) {
  | ZOpSeq(_, ZOperand(zoperand, (inner_prefix, inner_suffix))) =>
    let new_prefix = Seq.affix_affix(inner_prefix, prefix);
    let new_suffix = Seq.affix_affix(inner_suffix, suffix);
    ZOperand(zoperand, (new_prefix, new_suffix));
  | ZOpSeq(_, ZOperator(zoperator, (inner_prefix, inner_suffix))) =>
    let new_prefix = Seq.seq_affix(inner_prefix, prefix);
    let new_suffix = Seq.seq_affix(inner_suffix, suffix);
    ZOperator(zoperator, (new_prefix, new_suffix));
  };

let rec syn_move =
        (ctx: Contexts.t, u_gen: MetaVarGen.t, a: Action.t, zp: ZPat.t)
        : ActionOutcome.t(syn_success) =>
  switch (a) {
  /* Movement */
  | MoveTo(path) =>
    switch (CursorPath_Pat.follow(path, zp |> ZPat.erase)) {
    | None => Failed
    | Some(zp) => mk_syn_result(ctx, u_gen, zp)
    }
  | MoveToPrevHole =>
    switch (
      CursorPath_common.(prev_hole_steps(CursorPath_Pat.holes_z(zp, [])))
    ) {
    | None => Failed
    | Some(steps) =>
      switch (CursorPath_Pat.of_steps(steps, zp |> ZPat.erase)) {
      | None => Failed
      | Some(path) => syn_move(ctx, u_gen, MoveTo(path), zp)
      }
    }
  | MoveToNextHole =>
    switch (
      CursorPath_common.(next_hole_steps(CursorPath_Pat.holes_z(zp, [])))
    ) {
    | None => Failed
    | Some(steps) =>
      switch (CursorPath_Pat.of_steps(steps, zp |> ZPat.erase)) {
      | None => Failed
      | Some(path) => syn_move(ctx, u_gen, MoveTo(path), zp)
      }
    }
  | MoveLeft =>
    switch (zp |> ZPat.move_cursor_left) {
    | None => CursorEscaped(Before)
    | Some(zp) => mk_syn_result(ctx, u_gen, zp)
    }
  | MoveRight =>
    switch (zp |> ZPat.move_cursor_right) {
    | None => CursorEscaped(After)
    | Some(zp) => mk_syn_result(ctx, u_gen, zp)
    }
  | Construct(_)
  | Delete
  | Backspace
  | UpdateApPalette(_)
  | SwapUp
  | SwapDown
  | SwapLeft
  | SwapRight
  | Init =>
    failwith(
      __LOC__
      ++ ": expected movement action, got "
      ++ Sexplib.Sexp.to_string(Action.sexp_of_t(a)),
    )
  };

let rec ana_move =
        (
          ctx: Contexts.t,
          u_gen: MetaVarGen.t,
          a: Action.t,
          zp: ZPat.t,
          ty: HTyp.t,
        )
        : ActionOutcome.t(ana_success) =>
  switch (a) {
  /* Movement */
  | MoveTo(path) =>
    switch (CursorPath_Pat.follow(path, zp |> ZPat.erase)) {
    | None => Failed
    | Some(zp) => mk_ana_result(ctx, u_gen, zp, ty)
    }
  | MoveToPrevHole =>
    switch (
      CursorPath_common.(prev_hole_steps(CursorPath_Pat.holes_z(zp, [])))
    ) {
    | None => Failed
    | Some(steps) =>
      switch (CursorPath_Pat.of_steps(steps, zp |> ZPat.erase)) {
      | None => Failed
      | Some(path) => ana_move(ctx, u_gen, MoveTo(path), zp, ty)
      }
    }
  | MoveToNextHole =>
    switch (
      CursorPath_common.(next_hole_steps(CursorPath_Pat.holes_z(zp, [])))
    ) {
    | None => Failed
    | Some(steps) =>
      switch (CursorPath_Pat.of_steps(steps, zp |> ZPat.erase)) {
      | None => Failed
      | Some(path) => ana_move(ctx, u_gen, MoveTo(path), zp, ty)
      }
    }
  | MoveLeft =>
    switch (zp |> ZPat.move_cursor_left) {
    | None => CursorEscaped(Before)
    | Some(zp) => mk_ana_result(ctx, u_gen, zp, ty)
    }
  | MoveRight =>
    switch (zp |> ZPat.move_cursor_right) {
    | None => CursorEscaped(After)
    | Some(zp) => mk_ana_result(ctx, u_gen, zp, ty)
    }
  | Construct(_)
  | Delete
  | Backspace
  | UpdateApPalette(_)
  | SwapUp
  | SwapDown
  | SwapLeft
  | SwapRight
  | Init =>
    failwith(
      __LOC__
      ++ ": expected movement action, got "
      ++ Sexplib.Sexp.to_string(Action.sexp_of_t(a)),
    )
  };

let annotate_last_operand =
    (zseq: ZSeq.t('operand, 'operator, 'zoperand, 'zoperator), ann)
    : ZSeq.t('operand, 'operator, 'zoperand, 'zoperator) => {
  let annotate_op = op => UHPat.TypeAnn(NotInHole, op, ann);
  switch (zseq) {
  | ZOperand(zoperand, (prefix, E)) =>
    ZSeq.ZOperand(ZPat.TypeAnnZP(NotInHole, zoperand, ann), (prefix, E))
  | ZOperand(zoperand, (prefix, A(operator, seq))) =>
    ZOperand(
      zoperand,
      (prefix, A(operator, Seq.update_last_operand(annotate_op, seq))),
    )
  | ZOperator(zoperator, (prefix, S(operand, E))) =>
    ZOperator(zoperator, (prefix, S(annotate_op(operand), E)))
  | ZOperator(zoperator, (prefix, S(operand, A(operator, seq)))) =>
    ZOperator(
      zoperator,
      (
        prefix,
        S(operand, A(operator, Seq.update_last_operand(annotate_op, seq))),
      ),
    )
  };
};

let rec syn_perform =
        (ctx: Contexts.t, u_gen: MetaVarGen.t, a: Action.t, zp: ZPat.t)
        : ActionOutcome.t(syn_success) => {
  syn_perform_opseq(ctx, u_gen, a, zp);
}
and syn_perform_opseq =
    (
      ctx: Contexts.t,
      u_gen: MetaVarGen.t,
      a: Action.t,
      ZOpSeq(skel, zseq) as zopseq: ZPat.zopseq,
    )
    : ActionOutcome.t(syn_success) =>
  switch (a, zseq) {
  /* Invalid cursor positions */
  | (_, ZOperator((OnText(_) | OnDelim(_), _), _)) => Failed

  /* Invalid actions */
  | (UpdateApPalette(_), ZOperator(_)) => Failed

  /* Invalid swap actions */
  | (SwapUp | SwapDown, _) => Failed

  /* Movement */
  | (MoveTo(_) | MoveToPrevHole | MoveToNextHole | MoveLeft | MoveRight, _) =>
    syn_move(ctx, u_gen, a, zopseq)

  /* Deletion */

  | (Delete, ZOperator((OnOp(After as side), _), _))
  | (Backspace, ZOperator((OnOp(Before as side), _), _)) =>
    syn_perform_opseq(ctx, u_gen, Action_common.escape(side), zopseq)

  /* Delete before operator == Backspace after operator */
  | (Delete, ZOperator((OnOp(Before), op), surround)) =>
    let new_zp =
      ZOpSeq.ZOpSeq(
        skel,
        ZOperator((CursorPosition.OnOp(After), op), surround),
      );
    syn_perform_opseq(ctx, u_gen, Backspace, new_zp);

  /* ... + [k-1] +<| [k] + ... */
  | (Backspace, ZOperator((OnOp(After), _), surround)) =>
    let new_zseq = delete_operator(surround);
    Succeeded(mk_and_syn_fix_ZOpSeq(ctx, u_gen, new_zseq));

  /* ... + [k-1]  <|_ + [k+1] + ...  ==>   ... + [k-1]| + [k+1] + ... */
  | (
      Backspace,
      ZOperand(
        CursorP(_, EmptyHole(_)) as zhole,
        (A(Space, prefix_tl), suffix),
      ),
    )
      when ZPat.is_before_zoperand(zhole) =>
    let S(operand, new_prefix) = prefix_tl;
    let zoperand = operand |> ZPat.place_after_operand;
    let new_zseq = ZSeq.ZOperand(zoperand, (new_prefix, suffix));
    Succeeded(mk_and_syn_fix_ZOpSeq(ctx, u_gen, new_zseq));

  | (
      Backspace,
      ZOperand(
        TypeAnnZP(err, CursorP(_, EmptyHole(_)), ann) as zpann,
        (A(Space, prefix_tl), suffix),
      ),
    )
      when ZPat.is_before_zoperand(zpann) =>
    let S(operand, new_prefix) = prefix_tl;
    let zoperand =
      ZPat.TypeAnnZP(err, operand |> ZPat.place_after_operand, ann);
    let new_zseq = ZSeq.ZOperand(zoperand, (new_prefix, suffix));
    Succeeded(mk_and_syn_fix_ZOpSeq(ctx, u_gen, new_zseq));

  /* ... + [k-1] + _|>  [k+1] + ...  ==>   ... + [k-1] + |[k+1] + ... */
  | (
      Delete,
      ZOperand(
        CursorP(_, EmptyHole(_)) as zhole,
        (prefix, A(Space, suffix_tl)),
      ),
    )
      when ZPat.is_after_zoperand(zhole) =>
    let S(operand, new_suffix) = suffix_tl;
    let zoperand = operand |> ZPat.place_before_operand;
    let new_zseq = ZSeq.ZOperand(zoperand, (prefix, new_suffix));
    Succeeded(mk_and_syn_fix_ZOpSeq(ctx, u_gen, new_zseq));

  /* Construction */

  /* construction on operators either becomes movement... */
  | (Construct(SOp(SSpace)), ZOperator(zoperator, _))
      when ZPat.is_after_zoperator(zoperator) =>
    syn_perform_opseq(ctx, u_gen, MoveRight, zopseq)
  /* ...or construction after movement */
  | (Construct(_), ZOperator(zoperator, _)) =>
    let move_cursor =
      ZPat.is_before_zoperator(zoperator)
        ? ZPat.move_cursor_left : ZPat.move_cursor_right;
    switch (zopseq |> move_cursor) {
    | None => Failed
    | Some(zp) => syn_perform(ctx, u_gen, a, zp)
    };

  | (Construct(SOp(os)), ZOperand(zoperand, surround))
      when
        ZPat.is_before_zoperand(zoperand)
        && !shape_is_of_unop(os)
        || ZPat.is_after_zoperand(zoperand) =>
    switch (operator_of_shape(os)) {
    | None =>
      /* If the cursor is immeditely after a type annotation, and we're trying
       * to insert an operator that Pat doesn't recognize, delegate the action
       * to Typ.perform. Note that in the case of the one currently existing overlap,
       * SComma, this means that Pat gets priority, and one must insert parens around
       * a type annotation to express a product type.
       *  */
      switch (zoperand) {
      | TypeAnnZA(err, operand, zann) when ZTyp.is_after(zann) =>
        switch (Action_Typ.perform(a, zann)) {
        | Succeeded(new_zann) =>
          let new_zseq =
            ZSeq.ZOperand(ZPat.TypeAnnZA(err, operand, new_zann), surround);
          Succeeded(mk_and_syn_fix_ZOpSeq(ctx, u_gen, new_zseq));
        | _ => Failed
        }
      | _ => Failed
      }
    | Some(operator) =>
      let construct_operator =
        ZPat.is_before_zoperand(zoperand)
          ? construct_operator_before_zoperand
          : construct_operator_after_zoperand;
      let (zseq, u_gen) =
        construct_operator(u_gen, operator, zoperand, surround);
      Succeeded(mk_and_syn_fix_ZOpSeq(ctx, u_gen, zseq));
    }

  /* SwapLeft and SwapRight actions */
  | (SwapLeft, ZOperator(_))
  | (SwapRight, ZOperator(_)) => Failed

  | (SwapLeft, ZOperand(CursorP(_), (E, _))) => Failed
  | (
      SwapLeft,
      ZOperand(
        CursorP(_) as zoperand,
        (A(operator, S(operand, new_prefix)), suffix),
      ),
    ) =>
    let new_suffix = Seq.A(operator, S(operand, suffix));
    let new_zseq = ZSeq.ZOperand(zoperand, (new_prefix, new_suffix));
    Succeeded(mk_and_syn_fix_ZOpSeq(ctx, u_gen, new_zseq));
  | (SwapRight, ZOperand(CursorP(_), (_, E))) => Failed
  | (
      SwapRight,
      ZOperand(
        CursorP(_) as zoperand,
        (prefix, A(operator, S(operand, new_suffix))),
      ),
    ) =>
    let new_prefix = Seq.A(operator, S(operand, prefix));
    let new_zseq = ZSeq.ZOperand(zoperand, (new_prefix, new_suffix));
    Succeeded(mk_and_syn_fix_ZOpSeq(ctx, u_gen, new_zseq));

  /* Zipper */

  | (_, ZOperand(zoperand, (E, E))) =>
    syn_perform_operand(ctx, u_gen, a, zoperand)

  | (_, ZOperand(zoperand, (prefix, _) as surround)) =>
    let n = Seq.length_of_affix(prefix);
    switch (
      Statics_Pat.syn_nth_type_mode(ctx, n, zopseq |> ZPat.erase_zopseq)
    ) {
    | None => Failed
    | Some(Syn) =>
      switch (syn_perform_operand(ctx, u_gen, a, zoperand)) {
      | Failed => Failed
      | CursorEscaped(side) =>
        syn_perform_opseq(ctx, u_gen, Action_common.escape(side), zopseq)
      | Succeeded((zp, _, _, u_gen)) =>
        let zseq = resurround_z(zp, surround);
        Succeeded(mk_and_syn_fix_ZOpSeq(ctx, u_gen, zseq));
      }
    | Some(Ana(ty_zoperand)) =>
      switch (ana_perform_operand(ctx, u_gen, a, zoperand, ty_zoperand)) {
      | Failed => Failed
      | CursorEscaped(side) =>
        syn_perform_opseq(ctx, u_gen, Action_common.escape(side), zopseq)
      | Succeeded((zp, _, u_gen)) =>
        let new_zseq = resurround_z(zp, surround);
        Succeeded(mk_and_syn_fix_ZOpSeq(ctx, u_gen, new_zseq));
      }
    };
  | (Init, _) => failwith("Init action should not be performed.")
  }
and syn_perform_operand =
    (
      ctx: Contexts.t,
      u_gen: MetaVarGen.t,
      a: Action.t,
      zoperand: ZPat.zoperand,
    )
    : ActionOutcome.t(syn_success) => {
  switch (a, zoperand) {
  /* Invalid cursor positions */
  | (
      _,
      CursorP(
        OnText(_) | OnOp(_),
        EmptyHole(_) | Wild(_) | ListNil(_) | Parenthesized(_) | Inj(_) |
        TypeAnn(_),
      ) |
      CursorP(
        OnDelim(_) | OnOp(_),
        InvalidText(_, _) | Var(_) | IntLit(_) | FloatLit(_) | BoolLit(_),
      ) |
      CursorP(OnOp(After) | OnText(_) | OnDelim(_), UnaryOp(_)),
    ) =>
    Failed
  | (_, CursorP(cursor, operand))
      when !ZPat.is_valid_cursor_operand(cursor, operand) =>
    Failed

  /* Invalid actions */
  | (
      Construct(
        SApPalette(_) | SList | SLet | SLine | SLam | SCase | SCommentLine,
      ) |
      UpdateApPalette(_) |
      SwapUp |
      SwapDown,
      CursorP(_),
    ) =>
    Failed

  /* Movement */
  | (MoveTo(_) | MoveToPrevHole | MoveToNextHole | MoveLeft | MoveRight, _) =>
    syn_move(ctx, u_gen, a, ZOpSeq.wrap(zoperand))

  /* Backspace and Delete */

  | (Backspace, _) when ZPat.is_before_zoperand(zoperand) =>
    CursorEscaped(Before)
  | (Delete, _) when ZPat.is_after_zoperand(zoperand) =>
    CursorEscaped(After)

  | (Backspace, CursorP(_, EmptyHole(_) as operand)) =>
    let zp = ZOpSeq.wrap(ZPat.place_before_operand(operand));
    zp |> ZPat.is_after
      ? Succeeded((zp, Hole, ctx, u_gen)) : CursorEscaped(Before);
  | (Delete, CursorP(_, EmptyHole(_) as operand)) =>
    let zp = ZOpSeq.wrap(ZPat.place_after_operand(operand));
    zp |> ZPat.is_before
      ? Succeeded((zp, Hole, ctx, u_gen)) : CursorEscaped(After);

  /* ( _ <|)   ==>   ( _| ) */
  | (Backspace, CursorP(OnDelim(_, Before), _)) =>
    syn_perform(ctx, u_gen, MoveLeft, ZOpSeq.wrap(zoperand))
  /* (|> _ )   ==>   ( |_ ) */
  | (Delete, CursorP(OnDelim(_, After), _)) =>
    syn_perform(ctx, u_gen, MoveRight, ZOpSeq.wrap(zoperand))

  /* Delete before delimiter == Backspace after delimiter */
  | (Delete, CursorP(OnDelim(k, Before), operand)) =>
    let new_zp = ZOpSeq.wrap(ZPat.CursorP(OnDelim(k, After), operand));
    syn_perform(ctx, u_gen, Backspace, new_zp);

  | (Backspace, CursorP(OnDelim(_, After), ListNil(_) | Wild(_))) =>
    let (zhole, u_gen) = ZPat.new_EmptyHole(u_gen);
    let zp = ZOpSeq.wrap(zhole);
    Succeeded((zp, Hole, ctx, u_gen));

  | (Backspace, CursorP(OnOp(Before), UnaryOp(_))) => CursorEscaped(Before)
  | (Delete, CursorP(OnOp(Before), UnaryOp(_, _, child))) =>
    let new_zoperand =
      ZPat.set_err_status_zoperand(
        NotInHole,
        ZPat.place_before_operand(child),
      );
    let new_zp = ZOpSeq.wrap(new_zoperand);
    switch (Statics_Pat.syn_operand(ctx, ZPat.erase_zoperand(zoperand))) {
    | None => Failed
    | Some((ty, ctx)) => Succeeded((new_zp, ty, ctx, u_gen))
    };

  | (Backspace, CursorP(OnDelim(_ /* 0 */, After), TypeAnn(_, op, _))) =>
    Succeeded(
      Statics_Pat.syn_fix_holes_z(
        ctx,
        u_gen,
        op |> ZPat.place_after_operand |> ZOpSeq.wrap,
      ),
    )
  | (Delete, CursorP(OnText(j), InvalidText(_, t))) =>
    syn_delete_text(ctx, u_gen, j, t)
  | (Delete, CursorP(OnText(j), Var(_, _, x))) =>
    syn_delete_text(ctx, u_gen, j, x)
  | (Delete, CursorP(OnText(j), IntLit(_, n))) =>
    switch (znumlit_to_zunop(zoperand)) {
    | Some(zunop) => syn_perform_operand(ctx, u_gen, a, zunop)
    | None => syn_delete_text(ctx, u_gen, j, n)
    }
  | (Delete, CursorP(OnText(j), FloatLit(_, f))) =>
    switch (znumlit_to_zunop(zoperand)) {
    | Some(zunop) => syn_perform_operand(ctx, u_gen, a, zunop)
    | None => syn_delete_text(ctx, u_gen, j, f)
    }
  | (Delete, CursorP(OnText(j), BoolLit(_, b))) =>
    syn_delete_text(ctx, u_gen, j, string_of_bool(b))

  | (Backspace, CursorP(OnText(j), InvalidText(_, t))) =>
    syn_backspace_text(ctx, u_gen, j, t)
  | (Backspace, CursorP(OnText(j), Var(_, _, x))) =>
    syn_backspace_text(ctx, u_gen, j, x)
  | (Backspace, CursorP(OnText(j), IntLit(_, n))) =>
    switch (znumlit_to_zunop(zoperand)) {
    | Some(zunop) => syn_perform_operand(ctx, u_gen, a, zunop)
    | None => syn_backspace_text(ctx, u_gen, j, n)
    }
  | (Backspace, CursorP(OnText(j), FloatLit(_, f))) =>
    switch (znumlit_to_zunop(zoperand)) {
    | Some(zunop) => syn_perform_operand(ctx, u_gen, a, zunop)
    | None => syn_backspace_text(ctx, u_gen, j, f)
    }
  | (Backspace, CursorP(OnText(j), BoolLit(_, b))) =>
    syn_backspace_text(ctx, u_gen, j, string_of_bool(b))

  /* ( _ )<|  ==>  _| */
  /* (<| _ )  ==>  |_ */
  | (
      Backspace,
      CursorP(OnDelim(k, After), Parenthesized(body) | Inj(_, _, body)),
    ) =>
    let place_cursor = k == 0 ? ZPat.place_before : ZPat.place_after;
    Succeeded(Statics_Pat.syn_fix_holes_z(ctx, u_gen, body |> place_cursor));

  /* Construction */

  | (Construct(SOp(SSpace)), CursorP(OnDelim(_, After), _)) =>
    syn_perform_operand(ctx, u_gen, MoveRight, zoperand)
  | (Construct(_), CursorP(OnDelim(_, side), _))
      when
        !ZPat.is_before_zoperand(zoperand)
        && !ZPat.is_after_zoperand(zoperand) =>
    switch (
      syn_perform_operand(ctx, u_gen, Action_common.escape(side), zoperand)
    ) {
    | Failed
    | CursorEscaped(_) => Failed
    | Succeeded((zp, _, _, u_gen)) => syn_perform(ctx, u_gen, a, zp)
    }

  // TODO consider relaxing guards and
  // merging with regular op construction
  | (Construct(SOp(sop)), CursorP(OnText(j), InvalidText(_, t)))
      when
        !ZPat.is_before_zoperand(zoperand)
        && !ZPat.is_after_zoperand(zoperand) =>
    syn_split_text(ctx, u_gen, j, sop, t)
  | (Construct(SOp(sop)), CursorP(OnText(j), Var(_, _, x)))
      when
        !ZPat.is_before_zoperand(zoperand)
        && !ZPat.is_after_zoperand(zoperand) =>
    syn_split_text(ctx, u_gen, j, sop, x)
  | (Construct(SOp(sop)), CursorP(OnText(j), BoolLit(_, b)))
      when
        !ZPat.is_before_zoperand(zoperand)
        && !ZPat.is_after_zoperand(zoperand) =>
    syn_split_text(ctx, u_gen, j, sop, string_of_bool(b))
  | (Construct(SOp(sop)), CursorP(OnText(j), IntLit(_, n)))
      when
        !ZPat.is_before_zoperand(zoperand)
        && !ZPat.is_after_zoperand(zoperand) =>
    switch (znumlit_to_zunop(zoperand)) {
    | Some(zunop) when is_after_unop_of_negative_literal(zoperand) =>
      syn_perform_operand(ctx, u_gen, a, zunop)
    | _ => syn_split_text(ctx, u_gen, j, sop, n)
    }
  | (Construct(SOp(sop)), CursorP(OnText(j), FloatLit(_, f)))
      when
        !ZPat.is_before_zoperand(zoperand)
        && !ZPat.is_after_zoperand(zoperand) =>
    switch (znumlit_to_zunop(zoperand)) {
    | Some(zunop) when is_after_unop_of_negative_literal(zoperand) =>
      syn_perform_operand(ctx, u_gen, a, zunop)
    | _ => syn_split_text(ctx, u_gen, j, sop, f)
    }
  | (Construct(SChar(s)), CursorP(_, EmptyHole(_))) =>
    syn_insert_text(ctx, u_gen, (0, s), "")
  | (Construct(SChar(s)), CursorP(OnDelim(_, side), Wild(_))) =>
    let index =
      switch (side) {
      | Before => 0
      | After => 1
      };
    syn_insert_text(ctx, u_gen, (index, s), "_");
  | (Construct(SChar(s)), CursorP(OnText(j), InvalidText(_, t))) =>
    syn_insert_text(ctx, u_gen, (j, s), t)
  | (Construct(SChar(s)), CursorP(OnText(j), Var(_, _, x))) =>
    syn_insert_text(ctx, u_gen, (j, s), x)
  | (Construct(SChar(s)), CursorP(OnText(j), IntLit(_, n))) =>
    switch (znumlit_to_zunop(zoperand)) {
    | Some(zunop) => syn_perform_operand(ctx, u_gen, a, zunop)
    | None => syn_insert_text(ctx, u_gen, (j, s), n)
    }
  | (Construct(SChar(s)), CursorP(OnText(j), FloatLit(_, f))) =>
    switch (znumlit_to_zunop(zoperand)) {
    | Some(zunop) => syn_perform_operand(ctx, u_gen, a, zunop)
    | None => syn_insert_text(ctx, u_gen, (j, s), f)
    }
  | (Construct(SChar(s)), CursorP(OnText(j), BoolLit(_, b))) =>
    syn_insert_text(ctx, u_gen, (j, s), string_of_bool(b))
  | (Construct(SChar(_)), CursorP(_)) => Failed

  | (Construct(SListNil), CursorP(_, EmptyHole(_))) =>
    let zp = ZOpSeq.wrap(ZPat.place_after_operand(ListNil(NotInHole)));
    Succeeded((zp, List(Hole), ctx, u_gen));
  | (Construct(SListNil), CursorP(_, _)) => Failed

  | (Construct(SParenthesized), CursorP(_)) =>
    mk_syn_result(
      ctx,
      u_gen,
      ZOpSeq.wrap(ZPat.ParenthesizedZ(ZOpSeq.wrap(zoperand))),
    )

  | (Construct(SInj(side)), CursorP(_) as zbody) =>
    let zp = ZOpSeq.wrap(ZPat.InjZ(NotInHole, side, ZOpSeq.wrap(zbody)));
    switch (Statics_Pat.syn(ctx, zp |> ZPat.erase)) {
    | None => Failed
    | Some((body_ty, ctx)) =>
      let ty =
        switch (side) {
        | L => HTyp.Sum(body_ty, Hole)
        | R => HTyp.Sum(Hole, body_ty)
        };
      Succeeded((zp, ty, ctx, u_gen));
    };

  | (Construct(SOp(os)), CursorP(_)) =>
    switch (os) {
    | SMinus =>
      let unop = Unops_Pat.Negate;
      let ty_u = HTyp.Int;
      let (child, ctx, u_gen) =
        Statics_Pat.ana_fix_holes_operand(
          ctx,
          u_gen,
          ZPat.erase_zoperand(zoperand),
          ty_u,
        );
      let zchild = ZPat.place_before_operand(child);
      switch (
        syn_zunop_to_znumlit(
          ctx,
          u_gen,
          ZPat.UnaryOpZ(ErrStatus.NotInHole, unop, zchild),
        )
      ) {
      | Some((zoperand, ty, ctx, u_gen)) =>
        let new_zp = ZOpSeq.wrap(zoperand);
        Succeeded((new_zp, ty, ctx, u_gen));
      | None => Failed
      };
    | _ =>
      switch (operator_of_shape(os)) {
      | None => Failed
      | Some(operator) =>
        let construct_operator =
          ZPat.is_before_zoperand(zoperand)
            ? construct_operator_before_zoperand
            : construct_operator_after_zoperand;
        let (zseq, u_gen) =
          construct_operator(u_gen, operator, zoperand, (E, E));
        Succeeded(mk_and_syn_fix_ZOpSeq(ctx, u_gen, zseq));
      }
    }

  | (Construct(SAnn), CursorP(_)) =>
    let new_zann = ZOpSeq.wrap(ZTyp.place_before_operand(Hole));
    let new_zp =
      ZOpSeq.wrap(
        ZPat.TypeAnnZA(NotInHole, ZPat.erase_zoperand(zoperand), new_zann),
      );
    mk_syn_result(ctx, u_gen, new_zp);

  /* Invalid SwapLeft and SwapRight actions */
  | (SwapLeft | SwapRight, CursorP(_)) => Failed

  /* Zipper */
  | (_, ParenthesizedZ(zbody)) =>
    switch (syn_perform(ctx, u_gen, a, zbody)) {
    | Failed => Failed
    | CursorEscaped(side) =>
      syn_perform_operand(ctx, u_gen, Action_common.escape(side), zoperand)
    | Succeeded((zbody, ty, ctx, u_gen)) =>
      Succeeded((ZOpSeq.wrap(ZPat.ParenthesizedZ(zbody)), ty, ctx, u_gen))
    }
  | (_, UnaryOpZ(_, unop, zchild) as whole) =>
    let ty_u = Statics_Pat.syn_unop(ctx, unop);
    switch (a, unop, zchild) {
    | (Construct(SChar(".")), Negate, CursorP(_, e))
        when ZPat.is_before_zoperand(zchild) =>
      // convert int negation - to float negation -.
      let child = e;
      let (new_child, ctx, u_gen) =
        Statics_Pat.ana_fix_holes_operand(ctx, u_gen, child, Float);
      let new_zchild = ZPat.place_before_operand(new_child);
      switch (
        syn_zunop_to_znumlit(
          ctx,
          u_gen,
          ZPat.UnaryOpZ(NotInHole, FNegate, new_zchild),
        )
      ) {
      | Some((zoperand, ty, ctx, u_gen)) =>
        let new_zp = ZOpSeq.wrap(zoperand);
        Succeeded((new_zp, ty, ctx, u_gen));
      | None => Failed
      };
    | (Backspace, Negate, _) when ZPat.is_before_zoperand(zchild) =>
      let zchild = ZPat.set_err_status_zoperand(NotInHole, zchild);
      let new_zp = ZOpSeq.wrap(zchild);
      switch (Statics_Pat.syn_operand(ctx, ZPat.erase_zoperand(zchild))) {
      | None => Failed
      | Some((ty, ctx)) => Succeeded((new_zp, ty, ctx, u_gen))
      };
    | (Backspace, FNegate, _) when ZPat.is_before_zoperand(zchild) =>
      // convert -. float negation to - int negation
      let (new_child, ctx, u_gen) =
        Statics_Pat.ana_fix_holes_operand(
          ctx,
          u_gen,
          ZPat.erase_zoperand(zchild),
          Int,
        );
      let new_zchild = ZPat.place_before_operand(new_child);
      switch (
        syn_zunop_to_znumlit(
          ctx,
          u_gen,
          ZPat.UnaryOpZ(NotInHole, Negate, new_zchild),
        )
      ) {
      | Some((zoperand, ty, ctx, u_gen)) =>
        let new_zp = ZOpSeq.wrap(zoperand);
        Succeeded((new_zp, ty, ctx, u_gen));
      | None => Failed
      };
    | _ =>
      switch (ana_perform_operand(ctx, u_gen, a, zchild, ty_u)) {
      | Failed => Failed
      | CursorEscaped(side) =>
        syn_perform_operand(ctx, u_gen, Action_common.escape(side), whole)
      | Succeeded((zp, ctx, u_gen)) =>
        let wrap_in_parentheses = zp => {
          let new_zchild = ZPat.ParenthesizedZ(zp);
          let new_zp =
            ZOpSeq.wrap(ZPat.UnaryOpZ(NotInHole, unop, new_zchild));
          ActionOutcome.Succeeded((new_zp, ty_u, ctx, u_gen));
        };
        switch (zp) {
        | ZOpSeq(_, seq) =>
          switch (seq) {
          | ZOperand(zchild, (E, E)) =>
            switch (
              syn_zunop_to_znumlit(
                ctx,
                u_gen,
                ZPat.UnaryOpZ(NotInHole, unop, zchild),
              )
            ) {
            | Some((zoperand, ty, ctx, u_gen)) =>
              let new_zp = ZOpSeq.wrap(zoperand);
              Succeeded((new_zp, ty, ctx, u_gen));
            | None => Failed
            }
          | ZOperand(_) =>
            let is_before = ZPat.is_before_zopseq(zp);
            let first_operand = Seq.nth_operand(0, ZPat.erase_zseq(seq));
            let wrapped_first_operand =
              UHPat.UnaryOp(NotInHole, unop, first_operand);
            let seq' =
              Seq.update_nth_operand(
                0,
                wrapped_first_operand,
                ZPat.erase_zseq(seq),
              );
            let (zp, ty, ctx, u_gen) =
              mk_and_syn_fix_OpSeq(ctx, u_gen, seq');
            if (is_before) {
              let placed_before = ZPat.place_before_opseq(zp);
              let after_unop =
                OptUtil.get(
                  () => {failwith("could not place cursor after unop")},
                  ZPat.move_cursor_right_zopseq(placed_before),
                );
              Succeeded((after_unop, ty, ctx, u_gen));
            } else {
              Succeeded((ZPat.place_after_opseq(zp), ty, ctx, u_gen));
            };
          | _ => wrap_in_parentheses(zp)
          }
        };
      }
    };
  | (_, InjZ(_, side, zbody)) =>
    switch (syn_perform(ctx, u_gen, a, zbody)) {
    | Failed => Failed
    | CursorEscaped(side) =>
      syn_perform_operand(ctx, u_gen, Action_common.escape(side), zoperand)
    | Succeeded((zbody, ty1, ctx, u_gen)) =>
      let zp = ZOpSeq.wrap(ZPat.InjZ(NotInHole, side, zbody));
      let ty =
        switch (side) {
        | L => HTyp.Sum(ty1, Hole)
        | R => HTyp.Sum(Hole, ty1)
        };
      Succeeded((zp, ty, ctx, u_gen));
    }
  | (_, TypeAnnZP(_, zop, ann)) =>
    switch (syn_perform_operand(ctx, u_gen, a, zop)) {
    | Failed => Failed
    | CursorEscaped(side) =>
      syn_perform_operand(ctx, u_gen, Action_common.escape(side), zoperand)
    | Succeeded((ZOpSeq(_, zseq), _, ctx, u_gen)) =>
      let newseq = annotate_last_operand(zseq, ann);
      let (zpat, ty, ctx, u_gen) = mk_and_syn_fix_ZOpSeq(ctx, u_gen, newseq);
      Succeeded((zpat, ty, ctx, u_gen));
    }
  | (_, TypeAnnZA(_, op, zann)) =>
    switch (Action_Typ.perform(a, zann)) {
    | Failed => Failed
    | CursorEscaped(side) =>
      syn_perform_operand(ctx, u_gen, Action_common.escape(side), zoperand)
    | Succeeded(zann) =>
      let ty = UHTyp.expand(ZTyp.erase(zann));
      let (zpat, ctx, u_gen) =
        Statics_Pat.ana_fix_holes_z(
          ctx,
          u_gen,
          ZOpSeq.wrap(ZPat.TypeAnnZA(NotInHole, op, zann)),
          ty,
        );
      Succeeded((zpat, ty, ctx, u_gen));
    }
  | (Init, _) => failwith("Init action should not be performed.")
  };
}
and ana_perform =
    (
      ctx: Contexts.t,
      u_gen: MetaVarGen.t,
      a: Action.t,
      zp: ZPat.t,
      ty: HTyp.t,
    )
    : ActionOutcome.t(ana_success) => {
  ana_perform_opseq(ctx, u_gen, a, zp, ty);
}
and ana_perform_opseq =
    (
      ctx: Contexts.t,
      u_gen: MetaVarGen.t,
      a: Action.t,
      ZOpSeq(skel, zseq) as zopseq: ZPat.zopseq,
      ty: HTyp.t,
    )
    : ActionOutcome.t(ana_success) =>
  switch (a, zseq) {
  /* Invalid cursor positions */
  | (_, ZOperator((OnText(_) | OnDelim(_), _), _)) => Failed

  /* Invalid actions */
  | (UpdateApPalette(_), ZOperator(_)) => Failed
  | (SwapUp | SwapDown, _) => Failed

  /* Movement handled at top level */
  | (MoveTo(_) | MoveToPrevHole | MoveToNextHole | MoveLeft | MoveRight, _) =>
    ana_move(ctx, u_gen, a, zopseq, ty)

  /* Deletion */

  | (Delete, ZOperator((OnOp(After as side), _), _))
  | (Backspace, ZOperator((OnOp(Before as side), _), _)) =>
    ana_perform_opseq(ctx, u_gen, Action_common.escape(side), zopseq, ty)

  /* Delete before operator == Backspace after operator */
  | (Delete, ZOperator((OnOp(Before), op), surround)) =>
    let new_zp =
      ZOpSeq.ZOpSeq(
        skel,
        ZOperator((CursorPosition.OnOp(After), op), surround),
      );
    ana_perform_opseq(ctx, u_gen, Backspace, new_zp, ty);

  /* ... + [k-1] +<| [k] + ... */
  | (Backspace, ZOperator((OnOp(After), _), surround)) =>
    let new_zseq = delete_operator(surround);
    Succeeded(mk_and_ana_fix_ZOpSeq(ctx, u_gen, new_zseq, ty));

  /* ... + [k-1]  <|_ + [k+1] + ...  ==>   ... + [k-1]| + [k+1] + ... */
  | (
      Backspace,
      ZOperand(
        CursorP(_, EmptyHole(_)) as zhole,
        (A(Space, prefix_tl), suffix),
      ),
    )
      when ZPat.is_before_zoperand(zhole) =>
    let S(operand, new_prefix) = prefix_tl;
    let zoperand = operand |> ZPat.place_after_operand;
    let new_zseq = ZSeq.ZOperand(zoperand, (new_prefix, suffix));
    Succeeded(mk_and_ana_fix_ZOpSeq(ctx, u_gen, new_zseq, ty));

  | (
      Backspace,
      ZOperand(
        TypeAnnZP(err, CursorP(_, EmptyHole(_)), ann) as zpann,
        (A(Space, prefix_tl), suffix),
      ),
    )
      when ZPat.is_before_zoperand(zpann) =>
    let S(operand, new_prefix) = prefix_tl;
    let zoperand =
      ZPat.TypeAnnZP(err, operand |> ZPat.place_after_operand, ann);
    let new_zseq = ZSeq.ZOperand(zoperand, (new_prefix, suffix));
    Succeeded(mk_and_ana_fix_ZOpSeq(ctx, u_gen, new_zseq, ty));

  /* ... + [k-1] + _|>  [k+1] + ...  ==>   ... + [k-1] + |[k+1] + ... */
  | (
      Delete,
      ZOperand(
        CursorP(_, EmptyHole(_)) as zhole,
        (prefix, A(Space, suffix_tl)),
      ),
    )
      when ZPat.is_after_zoperand(zhole) =>
    let S(operand, new_suffix) = suffix_tl;
    let zoperand = operand |> ZPat.place_before_operand;
    let new_zseq = ZSeq.ZOperand(zoperand, (prefix, new_suffix));
    Succeeded(mk_and_ana_fix_ZOpSeq(ctx, u_gen, new_zseq, ty));

  /* Construction */

  /* construction on operators either becomes movement... */
  | (Construct(SOp(SSpace)), ZOperator(zoperator, _))
      when ZPat.is_after_zoperator(zoperator) =>
    ana_perform_opseq(ctx, u_gen, MoveRight, zopseq, ty)
  /* ...or construction after movement */
  | (Construct(_) as a, ZOperator(zoperator, _)) =>
    let move_cursor =
      ZPat.is_before_zoperator(zoperator)
        ? ZPat.move_cursor_left : ZPat.move_cursor_right;
    switch (zopseq |> move_cursor) {
    | None => Failed
    | Some(zp) => ana_perform(ctx, u_gen, a, zp, ty)
    };

  | (Construct(SOp(SComma)), _)
      when
        ZPat.is_after_zopseq(zopseq)
        && !(zopseq |> has_Comma)
        && List.length(HTyp.get_prod_elements(ty)) >= 2 =>
    let (opseq, ctx, u_gen) =
      Statics_Pat.ana_fix_holes_opseq(
        ctx,
        u_gen,
        zopseq |> ZPat.erase_zopseq,
        // safe because pattern guard
        ty |> HTyp.get_prod_elements |> List.hd,
      );
    let (new_zopseq, u_gen) = complete_tuple(u_gen, opseq, ty);
    Succeeded((new_zopseq, ctx, u_gen));
  | (Construct(SOp(os)), ZOperand(zoperand, surround))
      when
        ZPat.is_before_zoperand(zoperand)
        && !shape_is_of_unop(os)
        || ZPat.is_after_zoperand(zoperand) =>
    switch (operator_of_shape(os)) {
    | None =>
      /* If the cursor is immeditely after a type annotation, and we're trying
       * to insert an operator that Pat doesn't recognize, delegate the action
       * to Typ.perform. Note that in the case of the one currently existing overlap,
       * SComma, this means that Pat gets priority, and one must insert parens around
       * a type annotation to express a product type.
       *  */
      switch (zoperand) {
      | TypeAnnZA(err, operand, zann) when ZTyp.is_after(zann) =>
        switch (Action_Typ.perform(a, zann)) {
        | Succeeded(new_zann) =>
          let new_zseq =
            ZSeq.ZOperand(ZPat.TypeAnnZA(err, operand, new_zann), surround);
          let ty' = UHTyp.expand(ZTyp.erase(new_zann));
          Succeeded(mk_and_ana_fix_ZOpSeq(ctx, u_gen, new_zseq, ty'));
        | _ => Failed
        }
      | _ => Failed
      }
    | Some(operator) =>
      let construct_operator =
        ZPat.is_before_zoperand(zoperand)
          ? construct_operator_before_zoperand
          : construct_operator_after_zoperand;
      let (zseq, u_gen) =
        construct_operator(u_gen, operator, zoperand, surround);
      Succeeded(mk_and_ana_fix_ZOpSeq(ctx, u_gen, zseq, ty));
    }

  /* invalid swap actions */
  | (SwapLeft, ZOperator(_))
  | (SwapRight, ZOperator(_)) => Failed

  | (SwapLeft, ZOperand(CursorP(_), (E, _))) => Failed
  | (
      SwapLeft,
      ZOperand(
        CursorP(_) as zoperand,
        (A(operator, S(operand, new_prefix)), suffix),
      ),
    ) =>
    let new_suffix = Seq.A(operator, S(operand, suffix));
    let new_zseq = ZSeq.ZOperand(zoperand, (new_prefix, new_suffix));
    Succeeded(mk_and_ana_fix_ZOpSeq(ctx, u_gen, new_zseq, ty));
  | (SwapRight, ZOperand(CursorP(_), (_, E))) => Failed
  | (
      SwapRight,
      ZOperand(
        CursorP(_) as zoperand,
        (prefix, A(operator, S(operand, new_suffix))),
      ),
    ) =>
    let new_prefix = Seq.A(operator, S(operand, prefix));
    let new_zseq = ZSeq.ZOperand(zoperand, (new_prefix, new_suffix));
    Succeeded(mk_and_ana_fix_ZOpSeq(ctx, u_gen, new_zseq, ty));

  /* Zipper */
  | (_, ZOperand(zoperand, (E, E))) =>
    ana_perform_operand(ctx, u_gen, a, zoperand, ty)

  | (_, ZOperand(zoperand, (prefix, _) as surround)) =>
    let n = Seq.length_of_affix(prefix);
    switch (
      Statics_Pat.ana_nth_type_mode(ctx, n, zopseq |> ZPat.erase_zopseq, ty)
    ) {
    | None => Failed
    | Some(Syn) =>
      switch (syn_perform_operand(ctx, u_gen, a, zoperand)) {
      | Failed => Failed
      | CursorEscaped(side) =>
        ana_perform_opseq(ctx, u_gen, Action_common.escape(side), zopseq, ty)
      | Succeeded((zp, _, _, u_gen)) =>
        let zseq = resurround_z(zp, surround);
        Succeeded(mk_and_ana_fix_ZOpSeq(ctx, u_gen, zseq, ty));
      }
    | Some(Ana(ty_zoperand)) =>
      switch (ana_perform_operand(ctx, u_gen, a, zoperand, ty_zoperand)) {
      | Failed => Failed
      | CursorEscaped(side) =>
        ana_perform_opseq(ctx, u_gen, Action_common.escape(side), zopseq, ty)
      | Succeeded((zp, _, u_gen)) =>
        let new_zseq = resurround_z(zp, surround);
        Succeeded(mk_and_ana_fix_ZOpSeq(ctx, u_gen, new_zseq, ty));
      }
    };
  | (Init, _) => failwith("Init action should not be performed.")
  }
and ana_perform_operand =
    (
      ctx: Contexts.t,
      u_gen: MetaVarGen.t,
      a: Action.t,
      zoperand: ZPat.zoperand,
      ty: HTyp.t,
    )
    : ActionOutcome.t(ana_success) =>
  switch (a, zoperand) {
  /* Invalid cursor positions */
  | (
      _,
      CursorP(
        OnText(_) | OnOp(_),
        EmptyHole(_) | Wild(_) | ListNil(_) | Parenthesized(_) | Inj(_) |
        TypeAnn(_),
      ) |
      CursorP(
        OnDelim(_) | OnOp(_),
        InvalidText(_, _) | Var(_) | IntLit(_) | FloatLit(_) | BoolLit(_),
      ) |
      CursorP(OnOp(After) | OnDelim(_) | OnText(_), UnaryOp(_)),
    ) =>
    Failed
  | (_, CursorP(cursor, operand))
      when !ZPat.is_valid_cursor_operand(cursor, operand) =>
    Failed

  /* Invalid actions */
  | (
      Construct(
        SApPalette(_) | SList | SLet | SLine | SLam | SCase | SCommentLine,
      ) |
      UpdateApPalette(_) |
      SwapUp |
      SwapDown,
      CursorP(_),
    ) =>
    Failed

  /* switch to synthesis if in a hole */
  | (_, _) when ZPat.is_inconsistent(ZOpSeq.wrap(zoperand)) =>
    let zp = ZOpSeq.wrap(zoperand);
    let err = zp |> ZPat.erase |> UHPat.get_err_status;
    let zp' = zp |> ZPat.set_err_status(NotInHole);
    let p' = zp' |> ZPat.erase;
    switch (Statics_Pat.syn(ctx, p')) {
    | None => Failed
    | Some(_) =>
      switch (syn_perform(ctx, u_gen, a, zp')) {
      | (Failed | CursorEscaped(_)) as err => err
      | Succeeded((zp, ty', ctx, u_gen)) =>
        if (HTyp.consistent(ty, ty')) {
          Succeeded((zp, ctx, u_gen));
        } else if (HTyp.get_prod_arity(ty') != HTyp.get_prod_arity(ty)
                   && HTyp.get_prod_arity(ty) > 1) {
          let (u, u_gen) = MetaVarGen.next(u_gen);
          let new_zp = zp |> ZPat.set_err_status(InHole(WrongLength, u));
          Succeeded((new_zp, ctx, u_gen));
        } else {
          let new_zp = zp |> ZPat.set_err_status(err);
          Succeeded((new_zp, ctx, u_gen));
        }
      }
    };

  /* Movement */
  | (MoveTo(_) | MoveToPrevHole | MoveToNextHole | MoveLeft | MoveRight, _) =>
    ana_move(ctx, u_gen, a, ZOpSeq.wrap(zoperand), ty)

  /* Backspace and Delete */

  | (Backspace, _) when ZPat.is_before_zoperand(zoperand) =>
    CursorEscaped(Before)
  | (Delete, _) when ZPat.is_after_zoperand(zoperand) =>
    CursorEscaped(After)

  | (Backspace, CursorP(_, EmptyHole(_) as operand)) =>
    let zp = ZOpSeq.wrap(ZPat.place_before_operand(operand));
    zp |> ZPat.is_after
      ? Succeeded((zp, ctx, u_gen)) : CursorEscaped(Before);
  | (Delete, CursorP(_, EmptyHole(_) as operand)) =>
    let zp = ZOpSeq.wrap(ZPat.place_after_operand(operand));
    zp |> ZPat.is_before
      ? Succeeded((zp, ctx, u_gen)) : CursorEscaped(After);

  /* ( _ <|)   ==>   ( _| ) */
  | (Backspace, CursorP(OnDelim(_, Before), _)) =>
    ana_perform_operand(ctx, u_gen, MoveLeft, zoperand, ty)
  /* (|> _ )   ==>   ( |_ ) */
  | (Delete, CursorP(OnDelim(_, After), _)) =>
    ana_perform_operand(ctx, u_gen, MoveRight, zoperand, ty)

  /* Delete before delimiter == Backspace after delimiter */
  | (Delete, CursorP(OnDelim(k, Before), operand)) =>
    let new_zp = ZOpSeq.wrap(ZPat.CursorP(OnDelim(k, After), operand));
    ana_perform(ctx, u_gen, Backspace, new_zp, ty);

  | (Backspace, CursorP(OnDelim(_, After), Wild(_) | ListNil(_))) =>
    let (zhole, u_gen) = ZPat.new_EmptyHole(u_gen);
    let zp = ZOpSeq.wrap(zhole);
    Succeeded((zp, ctx, u_gen));

  | (Backspace, CursorP(OnOp(Before), UnaryOp(_, _, _))) =>
    CursorEscaped(Before)
  | (Delete, CursorP(OnOp(Before), UnaryOp(_, _, child))) =>
    let (child, ctx, u_gen) =
      Statics_Pat.ana_fix_holes_operand(ctx, u_gen, child, ty);
    let new_zp = ZOpSeq.wrap(ZPat.place_before_operand(child));
    Succeeded((new_zp, ctx, u_gen));

  | (Delete, CursorP(OnText(j), InvalidText(_, t))) =>
    ana_delete_text(ctx, u_gen, j, t, ty)
  | (Delete, CursorP(OnText(j), Var(_, _, x))) =>
    ana_delete_text(ctx, u_gen, j, x, ty)
  | (Delete, CursorP(OnText(j), IntLit(_, n))) =>
    switch (znumlit_to_zunop(zoperand)) {
    | Some(zunop) => ana_perform_operand(ctx, u_gen, a, zunop, ty)
    | None => ana_delete_text(ctx, u_gen, j, n, ty)
    }
  | (Delete, CursorP(OnText(j), FloatLit(_, f))) =>
    switch (znumlit_to_zunop(zoperand)) {
    | Some(zunop) => ana_perform_operand(ctx, u_gen, a, zunop, ty)
    | None => ana_delete_text(ctx, u_gen, j, f, ty)
    }
  | (Delete, CursorP(OnText(j), BoolLit(_, b))) =>
    ana_delete_text(ctx, u_gen, j, string_of_bool(b), ty)

  | (Backspace, CursorP(OnText(j), InvalidText(_, t))) =>
    ana_backspace_text(ctx, u_gen, j, t, ty)
  | (Backspace, CursorP(OnText(j), Var(_, _, x))) =>
    ana_backspace_text(ctx, u_gen, j, x, ty)
  | (Backspace, CursorP(OnText(j), IntLit(_, n))) =>
    switch (znumlit_to_zunop(zoperand)) {
    | Some(zunop) => ana_perform_operand(ctx, u_gen, a, zunop, ty)
    | None => ana_backspace_text(ctx, u_gen, j, n, ty)
    }
  | (Backspace, CursorP(OnText(j), FloatLit(_, f))) =>
    switch (znumlit_to_zunop(zoperand)) {
    | Some(zunop) => ana_perform_operand(ctx, u_gen, a, zunop, ty)
    | None => ana_backspace_text(ctx, u_gen, j, f, ty)
    }
  | (Backspace, CursorP(OnText(j), BoolLit(_, b))) =>
    ana_backspace_text(ctx, u_gen, j, string_of_bool(b), ty)

  /* ( _ )<|  ==>  _| */
  /* (<| _ )  ==>  |_ */
  | (
      Backspace,
      CursorP(OnDelim(k, After), Parenthesized(body) | Inj(_, _, body)),
    ) =>
    let place_cursor = k == 0 ? ZPat.place_before : ZPat.place_after;
    Succeeded(
      Statics_Pat.ana_fix_holes_z(ctx, u_gen, body |> place_cursor, ty),
    );
  | (Backspace, CursorP(OnDelim(_ /* 0 */, After), TypeAnn(_, op, _))) =>
    Succeeded(
      Statics_Pat.ana_fix_holes_z(
        ctx,
        u_gen,
        op |> ZPat.place_after_operand |> ZOpSeq.wrap,
        ty,
      ),
    )
  /* Construct */
  | (Construct(SOp(SSpace)), CursorP(OnDelim(_, After), _)) =>
    ana_perform_operand(ctx, u_gen, MoveRight, zoperand, ty)
  | (Construct(SAnn), CursorP(_)) =>
    let zty = ty |> UHTyp.contract |> ZTyp.place_before;
    let new_zp =
      ZOpSeq.wrap(
        ZPat.TypeAnnZA(NotInHole, ZPat.erase_zoperand(zoperand), zty),
      );
    mk_ana_result(ctx, u_gen, new_zp, ty);
  | (Construct(_) as a, CursorP(OnDelim(_, side), _))
      when
        !ZPat.is_before_zoperand(zoperand)
        && !ZPat.is_after_zoperand(zoperand) =>
    switch (
      ana_perform_operand(
        ctx,
        u_gen,
        Action_common.escape(side),
        zoperand,
        ty,
      )
    ) {
    | (Failed | CursorEscaped(_)) as err => err
    | Succeeded((zp, _, u_gen)) => ana_perform(ctx, u_gen, a, zp, ty)
    }

  // TODO consider relaxing guards and
  // merging with regular op construction
  | (Construct(SOp(sop)), CursorP(OnText(j), InvalidText(_, t)))
      when
        !ZPat.is_before_zoperand(zoperand)
        && !ZPat.is_after_zoperand(zoperand) =>
    ana_split_text(ctx, u_gen, j, sop, t, ty)
  | (Construct(SOp(sop)), CursorP(OnText(j), Var(_, _, x)))
      when
        !ZPat.is_before_zoperand(zoperand)
        && !ZPat.is_after_zoperand(zoperand) =>
    ana_split_text(ctx, u_gen, j, sop, x, ty)
  | (Construct(SOp(sop)), CursorP(OnText(j), BoolLit(_, b)))
      when
        !ZPat.is_before_zoperand(zoperand)
        && !ZPat.is_after_zoperand(zoperand) =>
    ana_split_text(ctx, u_gen, j, sop, string_of_bool(b), ty)
  | (Construct(SOp(sop)), CursorP(OnText(j), IntLit(_, n)))
      when
        !ZPat.is_before_zoperand(zoperand)
        && !ZPat.is_after_zoperand(zoperand) =>
    switch (znumlit_to_zunop(zoperand)) {
    | Some(zunop) when is_after_unop_of_negative_literal(zoperand) =>
      ana_perform_operand(ctx, u_gen, a, zunop, ty)
    | _ => ana_split_text(ctx, u_gen, j, sop, n, ty)
    }
  | (Construct(SOp(sop)), CursorP(OnText(j), FloatLit(_, f)))
      when
        !ZPat.is_before_zoperand(zoperand)
        && !ZPat.is_after_zoperand(zoperand) =>
    switch (znumlit_to_zunop(zoperand)) {
    | Some(zunop) when is_after_unop_of_negative_literal(zoperand) =>
      ana_perform_operand(ctx, u_gen, a, zunop, ty)
    | _ => ana_split_text(ctx, u_gen, j, sop, f, ty)
    }
  | (Construct(SChar(s)), CursorP(_, EmptyHole(_))) =>
    ana_insert_text(ctx, u_gen, (0, s), "", ty)
  | (Construct(SChar(s)), CursorP(OnDelim(_, side), Wild(_))) =>
    let index =
      switch (side) {
      | Before => 0
      | After => 1
      };
    ana_insert_text(ctx, u_gen, (index, s), "_", ty);
  | (Construct(SChar(s)), CursorP(OnText(j), InvalidText(_, t))) =>
    ana_insert_text(ctx, u_gen, (j, s), t, ty)
  | (Construct(SChar(s)), CursorP(OnText(j), Var(_, _, x))) =>
    ana_insert_text(ctx, u_gen, (j, s), x, ty)
  | (Construct(SChar(s)), CursorP(OnText(j), IntLit(_, n))) =>
    switch (znumlit_to_zunop(zoperand)) {
    | Some(zunop) => ana_perform_operand(ctx, u_gen, a, zunop, ty)
    | None => ana_insert_text(ctx, u_gen, (j, s), n, ty)
    }
  | (Construct(SChar(s)), CursorP(OnText(j), FloatLit(_, f))) =>
    switch (znumlit_to_zunop(zoperand)) {
    | Some(zunop) => ana_perform_operand(ctx, u_gen, a, zunop, ty)
    | None => ana_insert_text(ctx, u_gen, (j, s), f, ty)
    }
  | (Construct(SChar(s)), CursorP(OnText(j), BoolLit(_, b))) =>
    ana_insert_text(ctx, u_gen, (j, s), string_of_bool(b), ty)
  | (Construct(SChar(_)), CursorP(_)) => Failed

  | (Construct(SParenthesized), CursorP(_, EmptyHole(_) as hole))
      when List.length(HTyp.get_prod_elements(ty)) >= 2 =>
    let (zopseq, u_gen) = complete_tuple(u_gen, OpSeq.wrap(hole), ty);
    let new_zp = ZPat.ParenthesizedZ(zopseq) |> ZOpSeq.wrap;
    mk_ana_result(ctx, u_gen, new_zp, ty);
  | (Construct(SParenthesized), CursorP(_)) =>
    let new_zp = ZOpSeq.wrap(ZPat.ParenthesizedZ(ZOpSeq.wrap(zoperand)));
    mk_ana_result(ctx, u_gen, new_zp, ty);

  | (Construct(SInj(side)), CursorP(_)) =>
    switch (HTyp.matched_sum(ty)) {
    | Some((tyL, tyR)) =>
      let body_ty = InjSide.pick(side, tyL, tyR);
      let (zbody, ctx, u_gen) =
        Statics_Pat.ana_fix_holes_z(
          ctx,
          u_gen,
          ZOpSeq.wrap(zoperand),
          body_ty,
        );
      let zp = ZOpSeq.wrap(ZPat.InjZ(NotInHole, side, zbody));
      Succeeded((zp, ctx, u_gen));
    | None =>
      let (zbody, _, ctx, u_gen) =
        Statics_Pat.syn_fix_holes_z(ctx, u_gen, ZOpSeq.wrap(zoperand));
      let (u, u_gen) = u_gen |> MetaVarGen.next;
      let zp =
        ZOpSeq.wrap(ZPat.InjZ(InHole(TypeInconsistent, u), side, zbody));
      Succeeded((zp, ctx, u_gen));
    }

  | (Construct(SOp(os)), CursorP(_)) =>
    switch (os) {
    | SMinus =>
      let unop = Unops_Pat.Negate;
      let ty_u = Statics_Pat.syn_unop(ctx, unop);
      let (child, ctx, u_gen) =
        Statics_Pat.ana_fix_holes_operand(
          ctx,
          u_gen,
          ZPat.erase_zoperand(zoperand),
          ty_u,
        );
      let new_zchild = ZPat.place_before_operand(child);
      switch (
        ana_zunop_to_znumlit(
          ctx,
          u_gen,
          ZPat.UnaryOpZ(ErrStatus.NotInHole, unop, new_zchild),
          ty,
        )
      ) {
      | Some((zoperand, ctx, u_gen)) =>
        let new_zp = ZOpSeq.wrap(zoperand);
        Succeeded((new_zp, ctx, u_gen));
      | None => Failed
      };
    | _ =>
      switch (operator_of_shape(os)) {
      | None => Failed
      | Some(operator) =>
        let construct_operator =
          ZPat.is_before_zoperand(zoperand)
            ? construct_operator_before_zoperand
            : construct_operator_after_zoperand;
        let (zseq, u_gen) =
          construct_operator(u_gen, operator, zoperand, (E, E));
        Succeeded(mk_and_ana_fix_ZOpSeq(ctx, u_gen, zseq, ty));
      }
    }

  /* Invalid SwapLeft and SwapRight actions */
  | (SwapLeft | SwapRight, CursorP(_)) => Failed

  /* Zipper */
  | (_, ParenthesizedZ(zbody)) =>
    switch (ana_perform(ctx, u_gen, a, zbody, ty)) {
    | Failed => Failed
    | CursorEscaped(side) =>
      ana_perform_operand(
        ctx,
        u_gen,
        Action_common.escape(side),
        zoperand,
        ty,
      )
    | Succeeded((zbody, ctx, u_gen)) =>
      let zp = ZOpSeq.wrap(ZPat.ParenthesizedZ(zbody));
      Succeeded((zp, ctx, u_gen));
    }
  | (_, InjZ(_, side, zbody)) =>
    switch (HTyp.matched_sum(ty)) {
    | None => Failed
    | Some((tyL, tyR)) =>
      let body_ty = InjSide.pick(side, tyL, tyR);
      switch (ana_perform(ctx, u_gen, a, zbody, body_ty)) {
      | Failed => Failed
      | CursorEscaped(side) =>
        ana_perform_operand(
          ctx,
          u_gen,
          Action_common.escape(side),
          zoperand,
          ty,
        )
      | Succeeded((zbody, ctx, u_gen)) =>
        let zp = ZOpSeq.wrap(ZPat.InjZ(NotInHole, side, zbody));
        Succeeded((zp, ctx, u_gen));
      };
    }
  | (_, UnaryOpZ(_, unop, zchild) as whole) =>
    let ty_u = Statics_Pat.syn_unop(ctx, unop);
    switch (a, unop, zchild) {
    | (Construct(SChar(".")), Negate, CursorP(_, e))
        when ZPat.is_before_zoperand(zchild) =>
      // convert int negation - to float negation -.
      let (new_child, ctx, u_gen) =
        Statics_Pat.ana_fix_holes_operand(ctx, u_gen, e, Float);
      let new_zchild = ZPat.place_before_operand(new_child);
      switch (
        ana_zunop_to_znumlit(
          ctx,
          u_gen,
          ZPat.UnaryOpZ(ErrStatus.NotInHole, FNegate, new_zchild),
          ty,
        )
      ) {
      | Some((zoperand, ctx, u_gen)) =>
        let new_zp = ZOpSeq.wrap(zoperand);
        Succeeded((new_zp, ctx, u_gen));
      | None => Failed
      };
    | (Backspace, Negate, _) when ZPat.is_before_zoperand(zchild) =>
      let (operand, ctx, u_gen) =
        Statics_Pat.ana_fix_holes_operand(
          ctx,
          u_gen,
          ZPat.erase_zoperand(zchild),
          ty,
        );
      let new_zp = ZOpSeq.wrap(ZPat.place_before_operand(operand));
      Succeeded((new_zp, ctx, u_gen));
    | (Backspace, FNegate, _) when ZPat.is_before_zoperand(zchild) =>
      // convert float negation -. back to int negation -
      let (new_child, ctx, u_gen) =
        Statics_Pat.ana_fix_holes_operand(
          ctx,
          u_gen,
          ZPat.erase_zoperand(zchild),
          Int,
        );
      let new_zchild = ZPat.place_before_operand(new_child);
      switch (
        ana_zunop_to_znumlit(
          ctx,
          u_gen,
          ZPat.UnaryOpZ(ErrStatus.NotInHole, Negate, new_zchild),
          ty,
        )
      ) {
      | Some((zoperand, ctx, u_gen)) =>
        let new_zp = ZOpSeq.wrap(zoperand);
        Succeeded((new_zp, ctx, u_gen));
      | None => Failed
      };
    | _ =>
      switch (ana_perform_operand(ctx, u_gen, a, zchild, ty_u)) {
      | Failed => Failed
      | CursorEscaped(side) =>
        ana_perform_operand(
          ctx,
          u_gen,
          Action_common.escape(side),
          whole,
          ty,
        )
      | Succeeded((zp, ctx, u_gen)) =>
        let wrap_in_parentheses = zp => {
          let zchild = ZPat.ParenthesizedZ(zp);
          HTyp.consistent(ty, ty_u)
            ? {
              let new_zp =
                ZOpSeq.wrap(ZPat.UnaryOpZ(NotInHole, unop, zchild));
              ActionOutcome.Succeeded((new_zp, ctx, u_gen));
            }
            : {
              let (u, u_gen) = u_gen |> MetaVarGen.next;
              let new_zp =
                ZOpSeq.wrap(
                  ZPat.UnaryOpZ(InHole(TypeInconsistent, u), unop, zchild),
                );
              ActionOutcome.Succeeded((new_zp, ctx, u_gen));
            };
        };
        switch (zp) {
        | ZOpSeq(_, seq) =>
          switch (seq) {
          | ZOperand(zchild, (E, E)) =>
            switch (
              ana_zunop_to_znumlit(
                ctx,
                u_gen,
                ZPat.UnaryOpZ(ErrStatus.NotInHole, unop, zchild),
                ty,
              )
            ) {
            | Some((zoperand, ctx, u_gen)) =>
              let new_zp = ZOpSeq.wrap(zoperand);
              Succeeded((new_zp, ctx, u_gen));
            | None => Failed
            }
          | ZOperand(_) =>
            let is_before = ZPat.is_before_zopseq(zp);
            let first_operand = Seq.nth_operand(0, ZPat.erase_zseq(seq));
            let wrapped_first_operand =
              UHPat.UnaryOp(NotInHole, unop, first_operand);
            let seq' =
              Seq.update_nth_operand(
                0,
                wrapped_first_operand,
                ZPat.erase_zseq(seq),
              );
            let (zp, ctx, u_gen) =
              mk_and_ana_fix_OpSeq(ctx, u_gen, seq', ty);
            if (is_before) {
              let placed_before = ZPat.place_before_opseq(zp);
              let after_unop =
                OptUtil.get(
                  () => {failwith("could not place cursor after unop")},
                  ZPat.move_cursor_right_zopseq(placed_before),
                );
              Succeeded((after_unop, ctx, u_gen));
            } else {
              Succeeded((ZPat.place_after_opseq(zp), ctx, u_gen));
            };
          | _ => wrap_in_parentheses(zp)
          }
        };
      }
    };

  | (_, TypeAnnZP(_, zop, ann)) =>
    switch (ana_perform_operand(ctx, u_gen, a, zop, ty)) {
    | Failed => Failed
    | CursorEscaped(side) =>
      ana_perform_operand(
        ctx,
        u_gen,
        Action_common.escape(side),
        zoperand,
        ty,
      )
    | Succeeded((ZOpSeq(_, zseq), ctx, u_gen)) =>
      // NOTE: Type annotations cannot be directly implemented as infix operators
      // since the sorts on both sides differ. Thus an annotation is not parsed
      // systematically as part of an opseq, so we have to reassociate the annotation
      // onto the trailing operand.
      let newseq = annotate_last_operand(zseq, ann);
      let (zpat, ctx, u_gen) = mk_and_ana_fix_ZOpSeq(ctx, u_gen, newseq, ty);
      Succeeded((zpat, ctx, u_gen));
    }
  | (_, TypeAnnZA(err, op, zann)) =>
    switch (Action_Typ.perform(a, zann)) {
    | Failed => Failed
    | CursorEscaped(side) =>
      ana_perform_operand(
        ctx,
        u_gen,
        Action_common.escape(side),
        zoperand,
        ty,
      )
    | Succeeded(zann) =>
      let ty' = UHTyp.expand(ZTyp.erase(zann));
      let (new_op, ctx, u_gen) =
        Statics_Pat.ana_fix_holes_operand(ctx, u_gen, op, ty');
      let new_zopseq = ZOpSeq.wrap(ZPat.TypeAnnZA(err, new_op, zann));
      if (HTyp.consistent(ty, ty')) {
        Succeeded((new_zopseq, ctx, u_gen));
      } else {
        let (new_zopseq, u_gen) = new_zopseq |> ZPat.mk_inconsistent(u_gen);
        Succeeded((new_zopseq, ctx, u_gen));
      };
    }
  /* Subsumption */
  | (Construct(SListNil), _) =>
    switch (syn_perform_operand(ctx, u_gen, a, zoperand)) {
    | (Failed | CursorEscaped(_)) as err => err
    | Succeeded((zp, ty', ctx, u_gen)) =>
      if (HTyp.consistent(ty, ty')) {
        Succeeded((zp, ctx, u_gen));
      } else {
        let (zp, u_gen) = zp |> ZPat.mk_inconsistent(u_gen);
        Succeeded((zp, ctx, u_gen));
      }
    }
  | (Init, _) => failwith("Init action should not be performed.")
  };
