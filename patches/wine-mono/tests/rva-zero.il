// SPDX-License-Identifier: MIT
// prepare-rva-fixture.py changes Zero's FieldRVA to zero and marks Missing
// HasFieldRVA without adding a row. Never run the unprepared assembly.
.assembly extern mscorlib {}
.assembly MonoRvaZero {}
.module MonoRvaZero.exe

.data ZeroData = bytearray (00 00)
.data RegularData = int32 (0x12345678)

.class public auto ansi RvaFields extends [mscorlib]System.Object
{
    .field public static initonly uint16 Zero at ZeroData
    .field public static initonly int32 Regular at RegularData
}

.class public auto ansi MissingField extends [mscorlib]System.Object
{
    .field public static uint8 Missing

    .method public static int32 Check() cil managed
    {
        .maxstack 2
        ldsfld uint8 MissingField::Missing
        brtrue Failed
        ldc.i4.7
        stsfld uint8 MissingField::Missing
        ldsfld uint8 MissingField::Missing
        ldc.i4.7
        bne.un Failed
        ldc.i4.0
        ret
    Failed:
        ldc.i4.5
        ret
    }
}

.class public auto ansi Program extends [mscorlib]System.Object
{
    .method private static pinvokeimpl("kernel32.dll" as "GetModuleHandleW" winapi)
        native int GetModuleHandle(native int name) cil managed preservesig {}

    .method public static int32 Main(string[] args) cil managed
    {
        .entrypoint
        .maxstack 3
        .locals init (uint8[] bytes)

        // ldsflda must identify the executable's loaded image, not static storage.
        ldsflda uint16 RvaFields::Zero
        conv.u
        ldc.i4.0
        conv.i
        call native int Program::GetModuleHandle(native int)
        bne.un BadAddress

        // ldsfld must read the DOS signature from the same address.
        ldsfld uint16 RvaFields::Zero
        ldc.i4 0x5a4d
        bne.un BadValue

        // Preserve ordinary nonzero RVA fields.
        ldsfld int32 RvaFields::Regular
        ldc.i4 0x12345678
        bne.un BadRegular

        // Exercise RuntimeHelpers as well as the JIT's direct field loads.
        ldc.i4.2
        newarr [mscorlib]System.Byte
        stloc.0
        ldloc.0
        ldtoken field uint16 RvaFields::Zero
        call void [mscorlib]System.Runtime.CompilerServices.RuntimeHelpers::InitializeArray(
            class [mscorlib]System.Array, valuetype [mscorlib]System.RuntimeFieldHandle)
        ldloc.0
        ldc.i4.0
        ldelem.u1
        ldc.i4 0x4d
        bne.un BadArray
        ldloc.0
        ldc.i4.1
        ldelem.u1
        ldc.i4 0x5a
        bne.un BadArray

        // Optional Mono-only control for its existing missing-row fallback.
        ldarg.0
        ldlen
        brfalse Success
        call int32 MissingField::Check()
        ret
    Success:
        ldc.i4.0
        ret
    BadAddress:
        ldc.i4.1
        ret
    BadValue:
        ldc.i4.2
        ret
    BadRegular:
        ldc.i4.3
        ret
    BadArray:
        ldc.i4.4
        ret
    }
}
