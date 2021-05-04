open Sexplib.Std;

[@deriving sexp]
type t('a) = list((Tag.t, 'a));

let compare_tags = ((tag1, _): (Tag.t, 'a), (tag2, _): (Tag.t, 'a)): int =>
  Tag.compare(tag1, tag2);

let equal_bindings =
    (
      val_equal: ('a, 'a) => bool,
      (tag1: Tag.t, val1: 'a),
      (tag2: Tag.t, val2: 'a),
    )
    : bool =>
  Tag.eq(tag1, tag2) && val_equal(val1, val2);

let equal = (val_equal: ('a, 'a) => bool, map1: t('a), map2: t('a)): bool => {
  map1 === map2
  || {
    let map1 = List.fast_sort(compare_tags, map1);
    let map2 = List.fast_sort(compare_tags, map2);
    ListUtil.equal(equal_bindings(val_equal), map1, map2);
  };
};

// module Sexp = Sexplib.Sexp;

// [@deriving sexp]
// type binding('v) = (Tag.t, 'v);

// let sexp_of_t = (sexp_of_v: 'v => Sexp.t, map: t('v)): Sexp.t =>
//   map |> bindings |> sexp_of_list(sexp_of_binding(sexp_of_v));
// let t_of_sexp = (v_of_sexp: Sexp.t => 'v, sexp: Sexp.t): t('v) =>
//   sexp |> list_of_sexp(binding_of_sexp(v_of_sexp)) |> List.to_seq |> of_seq;
