#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Set up the two unusual metadata cases in our own regression fixture."""

import argparse
from pathlib import Path

import dnfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    if args.input.resolve() == args.output.resolve() or args.output.exists():
        parser.error('output must be a new file, not the input')

    pe = dnfile.dnPE(str(args.input))
    try:
        if not pe.net or not pe.net.mdtables.Assembly:
            parser.error('not a managed assembly')
        tables = pe.net.mdtables
        if str(tables.Assembly.rows[0].Name) != 'MonoRvaZero':
            parser.error('only the MonoRvaZero test assembly is accepted')

        fields = {(str(typ.TypeName), str(field.row.Name)): field
                  for typ in tables.TypeDef for field in typ.FieldList}
        zero = fields['RvaFields', 'Zero']
        missing = fields['MissingField', 'Missing']
        zero_rows = [row for row in tables.FieldRva
                     if row.Field.row_index == zero.row_index]
        if len(zero_rows) != 1 or not zero_rows[0].Rva:
            parser.error('expected one nonzero RVA for the Zero placeholder')
        if any(row.Field.row_index == missing.row_index for row in tables.FieldRva):
            parser.error('Missing must not have a FieldRVA row')

        zero_rows[0].struct.Rva = 0
        missing.row.struct.Flags |= 0x0100  # FieldAttributes.HasFieldRVA
        # dnfile parses metadata rows separately from pefile's write-back list.
        for row in (zero_rows[0], missing.row):
            pe.set_bytes_at_offset(row.struct.get_file_offset(), row.struct.__pack__())
        with args.output.open('xb') as output:
            output.write(pe.write())
    finally:
        pe.close()


if __name__ == '__main__':
    main()
