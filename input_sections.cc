#include "mold.h"

#include <limits>

template <typename E>
void InputSection<E>::copy_buf(Context<E> &ctx) {
  if (shdr.sh_type == SHT_NOBITS || shdr.sh_size == 0)
    return;

  // Copy data
  u8 *base = ctx.buf + output_section->shdr.sh_offset + offset;
  memcpy(base, contents.data(), contents.size());

  // Apply relocations
  if (shdr.sh_flags & SHF_ALLOC)
    apply_reloc_alloc(ctx, base);
  else
    apply_reloc_nonalloc(ctx, base);
}

template <typename E>
static i64 get_output_type(Context<E> &ctx) {
  if (ctx.arg.shared)
    return 0;
  if (ctx.arg.pie)
    return 1;
  return 2;
}

template <typename E>
static i64 get_sym_type(Context<E> &ctx, Symbol<E> &sym) {
  if (sym.is_absolute(ctx))
    return 0;
  if (!sym.is_imported)
    return 1;
  if (sym.get_type() != STT_FUNC)
    return 2;
  return 3;
}

template <typename E>
void InputSection<E>::dispatch(Context<E> &ctx, Action table[3][4],
                               u16 rel_type, i64 i) {
  std::span<ElfRel<E>> rels = get_rels(ctx);
  const ElfRel<E> &rel = rels[i];
  Symbol<E> &sym = *file.symbols[rel.r_sym];
  bool is_readonly = !(shdr.sh_flags & SHF_WRITE);
  Action action = table[get_output_type(ctx)][get_sym_type(ctx, sym)];

  switch (action) {
  case NONE:
    rel_types[i] = rel_type;
    return;
  case ERROR:
    break;
  case COPYREL:
    if (!ctx.arg.z_copyreloc)
      break;
    if (sym.esym().st_visibility == STV_PROTECTED)
      Error(ctx) << *this << ": cannot make copy relocation for "
                 << " protected symbol '" << sym << "', defined in "
                 << *sym.file;
    sym.flags |= NEEDS_COPYREL;
    rel_types[i] = rel_type;
    return;
  case PLT:
    sym.flags |= NEEDS_PLT;
    rel_types[i] = rel_type;
    return;
  case DYNREL:
    if (is_readonly)
      break;
    sym.flags |= NEEDS_DYNSYM;
    rel_types[i] = R_DYN;
    file.num_dynrel++;
    return;
  case BASEREL:
    if (is_readonly)
      break;
    rel_types[i] = R_BASEREL;
    file.num_dynrel++;
    return;
  default:
    unreachable(ctx);
  }

  Error(ctx) << *this << ": " << rel_to_string<E>(rel.r_type)
             << " relocation against symbol `" << sym
             << "' can not be used; recompile with -fPIE";
}

template class InputSection<X86_64>;
template class InputSection<I386>;
