type t = {
  cardstacks: Cardstacks.t,
  /* UI state */
  compute_results: bool,
  selected_example: option(UHExp.t),
  is_cell_focused: bool,
  left_sidebar_open: bool,
  right_sidebar_open: bool,
  show_contenteditable: bool,
  show_presentation: bool,
  all_hidden_history_expand: bool,
  undo_history: UndoHistory.t,
};

let cutoff = (m1, m2) => m1 === m2;

let cardstack_info = [
  TutorialCards.cardstack,
  // RCStudyCards.cardstack,
];

let init = (): t => {
  let cardstacks = Cardstacks.mk(cardstack_info);
  let undo_history: UndoHistory.t = {
    let (cursor_term, prev_is_empty_line, next_is_empty_line) =
      UndoHistory.get_cursor_info(cardstacks);
    let undo_history_entry: UndoHistory.undo_history_entry = {
      cardstacks,
      cursor_term_info: {
        cursor_term_before: cursor_term,
        cursor_term_after: cursor_term,
        prev_is_empty_line,
        next_is_empty_line,
      },
      previous_action: Construct(SOp(SSpace)),
      edit_action: Ignore,
    };
    let timestamp = Unix.time();
    let undo_history_group: UndoHistory.undo_history_group = {
      group_entries: ([], undo_history_entry, []),
      is_expanded: false,
      timestamp,
      display_timestamp: false,
    };
    {groups: ([], undo_history_group, []), latest_timestamp: timestamp};
  };
  {
    cardstacks,
    undo_history,
    compute_results: true,
    selected_example: None,
    is_cell_focused: false,
    left_sidebar_open: false,
    right_sidebar_open: true,
    show_contenteditable: false,
    show_presentation: false,
    all_hidden_history_expand: false,
  };
};

let get_program = (model: t): Program.t =>
  model.cardstacks |> Cardstacks.get_program;
let put_program = (program: Program.t, model: t): t => {
  ...model,
  cardstacks: model.cardstacks |> Cardstacks.put_program(program),
};
let map_program = (f: Program.t => Program.t, model: t): t => {
  let new_program = f(model |> get_program);
  model |> put_program(new_program);
};

let get_undo_history = (model: t): UndoHistory.t => model.undo_history;
let put_undo_history = (history: UndoHistory.t, model: t): t => {
  ...model,
  undo_history: history,
};

let focus_cell = model => {...model, is_cell_focused: true};
let blur_cell = model => {...model, is_cell_focused: false};

let get_cardstacks = model => model.cardstacks;
let put_cardstacks = (cardstacks, model) => {...model, cardstacks};
let map_cardstacks = (f: Cardstacks.t => Cardstacks.t, model: t): t => {
  let new_cardstacks = f(model |> get_cardstacks);
  model |> put_cardstacks(new_cardstacks);
};

let get_cardstack = model => model |> get_cardstacks |> Cardstacks.get_z;
let get_card = model => model |> get_cardstack |> Cardstack.get_z;

let prev_card = model => {
  model
  |> map_cardstacks(Cardstacks.map_z(Cardstack.prev_card))
  |> focus_cell;
};
let next_card = model => {
  model
  |> map_cardstacks(Cardstacks.map_z(Cardstack.next_card))
  |> focus_cell;
};

let perform_edit_action = (a: Action.t, model: t): t => {
  let new_program = model |> get_program |> Program.perform_edit_action(a);
  model
  |> put_program(new_program)
  |> put_undo_history(
       {
         let history = model |> get_undo_history;
         let prev_cardstacks = model |> get_cardstacks;
         let new_cardstacks =
           model |> put_program(new_program) |> get_cardstacks;
         UndoHistory.push_edit_state(
           history,
           prev_cardstacks,
           new_cardstacks,
           a,
         );
       },
     );
};

let move_to_hole = (u: MetaVar.t, model: t): t =>
  model |> map_program(Program.move_to_hole(u));

let select_hole_instance = (inst: HoleInstance.t, model: t): t =>
  model |> map_program(Program.put_selected_instance(inst));

let toggle_left_sidebar = (model: t): t => {
  ...model,
  left_sidebar_open: !model.left_sidebar_open,
};
let toggle_right_sidebar = (model: t): t => {
  ...model,
  right_sidebar_open: !model.right_sidebar_open,
};

let load_example = (model: t, e: UHExp.t): t =>
  model
  |> put_program(
       Program.mk(
         Statics.Exp.fix_and_renumber_holes_z(
           Contexts.empty,
           ZExp.place_before(e),
         ),
       ),
     );

let load_cardstack = (model, idx) => {
  model |> map_cardstacks(Cardstacks.load_cardstack(idx)) |> focus_cell;
};

let undo = (model: t): t => {
  let new_groups = {
    let cur_group = ZList.prj_z(model.undo_history.groups);
    /* shift to previous state in the same group */
    switch (ZList.shift_next(cur_group.group_entries)) {
    | None =>
      /*if current group doesn't have previous state, shfit to previous group*/
      switch (ZList.shift_next(model.undo_history.groups)) {
      | None => model.undo_history.groups
      | Some(new_groups) =>
        let new_group = ZList.prj_z(new_groups);
        let new_group': UndoHistory.undo_history_group = {
          ...new_group,
          group_entries: ZList.shift_begin(new_group.group_entries), /*pointer may be in the wrong position after clicking an arbitrary entry in the history panel*/
          is_expanded: true,
        }; /* is_expanded=true because the selected group should be expanded*/
        ZList.replace_z(new_group', new_groups);
      }
    | Some(new_group_entries) =>
      let new_group: UndoHistory.undo_history_group = {
        ...cur_group,
        group_entries: new_group_entries,
        is_expanded: true,
      };
      ZList.replace_z(new_group, model.undo_history.groups); /* is_expanded=true because the selected group should be expanded*/
    };
  };
  let cur_group' = ZList.prj_z(new_groups);
  let new_cardstacks = ZList.prj_z(cur_group'.group_entries).cardstacks;
  let model' = model |> put_cardstacks(new_cardstacks);
  {
    ...model',
    undo_history: {
      ...model.undo_history,
      groups: new_groups,
    },
  };
};

let redo = (model: t): t => {
  let new_groups = {
    let cur_group = ZList.prj_z(model.undo_history.groups);
    /* shift to previous state in the same group */
    switch (ZList.shift_prev(cur_group.group_entries)) {
    | None =>
      /*if current group doesn't have previous state, shfit to previous group*/
      switch (ZList.shift_prev(model.undo_history.groups)) {
      | None => model.undo_history.groups
      | Some(new_groups) =>
        let new_group = ZList.prj_z(new_groups);
        let new_group': UndoHistory.undo_history_group = {
          ...new_group,
          group_entries: ZList.shift_end(new_group.group_entries), /*pointer may be in the wrong position after clicking an arbitrary entry in the history panel*/
          is_expanded: true,
        }; /* is_expanded=true because this group should be expanded when redo*/
        ZList.replace_z(new_group', new_groups);
      }
    | Some(new_group_entries) =>
      let new_group: UndoHistory.undo_history_group = {
        ...cur_group,
        group_entries: new_group_entries,
        is_expanded: true,
      };
      ZList.replace_z(new_group, model.undo_history.groups); /* is_expanded=true because the selected group should be expanded*/
    };
  };
  let cur_group' = ZList.prj_z(new_groups);
  let new_cardstacks = ZList.prj_z(cur_group'.group_entries).cardstacks;
  let model' = model |> put_cardstacks(new_cardstacks);
  {
    ...model',
    undo_history: {
      ...model.undo_history,
      groups: new_groups,
    },
  };
};
