// ResolveType/ResolveMember must preserve the type encoded in a TypeSpec.
// Read ldtoken operands from method bodies; do not assume token numbering.
.assembly extern mscorlib
{
    .ver 4:0:0:0
    .publickeytoken = (B7 7A 5C 56 19 34 E0 89)
}
.assembly ResolveTypeSpec {}
.module ResolveTypeSpec.exe

.class public auto ansi Sources`1<T> extends [mscorlib]System.Object
{
    // These methods only carry tokens and signatures; they are not called.
    .method public static uint8& ByteRef() cil managed
    {
        .maxstack 1
        ldtoken uint8&
        pop
        ldnull
        throw
    }
    .method public static int32& IntRef() cil managed
    {
        .maxstack 1
        ldtoken int32&
        pop
        ldnull
        throw
    }
    .method public static uint8* Pointer() cil managed
    {
        .maxstack 1
        ldtoken uint8*
        pop
        ldnull
        throw
    }
    .method public static uint8[] Array() cil managed
    {
        .maxstack 1
        ldtoken uint8[]
        pop
        ldnull
        throw
    }
    .method public static !T& TypeParameter() cil managed
    {
        .maxstack 1
        ldtoken !T&
        pop
        ldnull
        throw
    }
    .method public static !!U& MethodParameter<U>() cil managed
    {
        .maxstack 1
        ldtoken !!U&
        pop
        ldnull
        throw
    }
    .method public static uint8 Value() cil managed
    {
        .maxstack 1
        ldtoken uint8
        pop
        ldnull
        throw
    }
    .method public static class Test Definition() cil managed
    {
        .maxstack 1
        ldtoken Test
        pop
        ldnull
        throw
    }
}

.class public auto ansi Test extends [mscorlib]System.Object
{
    .method public static int32 Main() cil managed
    {
        .entrypoint
        .maxstack 5
        .locals init (
            class [mscorlib]System.Type source,
            class [mscorlib]System.Reflection.MethodInfo[] methods,
            int32 index,
            class [mscorlib]System.Reflection.MethodInfo method,
            class [mscorlib]System.Type[] typeArgs,
            class [mscorlib]System.Type[] methodArgs,
            class [mscorlib]System.Reflection.Module module,
            int32 token,
            class [mscorlib]System.Type expected)

        ldtoken class Sources`1<uint8>
        call class [mscorlib]System.Type [mscorlib]System.Type::GetTypeFromHandle(valuetype [mscorlib]System.RuntimeTypeHandle)
        stloc.0
        ldloc.0
        callvirt instance class [mscorlib]System.Type[] [mscorlib]System.Type::GetGenericArguments()
        stloc.s typeArgs
        ldloc.0
        ldc.i4.s 26 // DeclaredOnly | Static | Public
        callvirt instance class [mscorlib]System.Reflection.MethodInfo[] [mscorlib]System.Type::GetMethods(valuetype [mscorlib]System.Reflection.BindingFlags)
        stloc.1
        ldloc.1
        ldlen
        conv.i4
        ldc.i4.8
        bne.un BAD_COUNT
        br NEXT

    LOOP:
        ldloc.1
        ldloc.2
        ldelem.ref
        stloc.3
        ldloc.3
        callvirt instance bool [mscorlib]System.Reflection.MethodBase::get_IsGenericMethodDefinition()
        brfalse READY
        ldloc.3
        ldc.i4.1
        newarr [mscorlib]System.Type
        dup
        ldc.i4.0
        ldtoken int32
        call class [mscorlib]System.Type [mscorlib]System.Type::GetTypeFromHandle(valuetype [mscorlib]System.RuntimeTypeHandle)
        stelem.ref
        callvirt instance class [mscorlib]System.Reflection.MethodInfo [mscorlib]System.Reflection.MethodInfo::MakeGenericMethod(class [mscorlib]System.Type[])
        stloc.3
    READY:
        ldloc.3
        callvirt instance class [mscorlib]System.Type[] [mscorlib]System.Reflection.MethodBase::GetGenericArguments()
        stloc.s methodArgs
        ldloc.3
        callvirt instance class [mscorlib]System.Reflection.Module [mscorlib]System.Reflection.MemberInfo::get_Module()
        stloc.s module
        ldloc.3
        callvirt instance class [mscorlib]System.Type [mscorlib]System.Reflection.MethodInfo::get_ReturnType()
        stloc.s expected

        // The first instruction is ldtoken; its operand starts at byte 1.
        ldloc.3
        callvirt instance class [mscorlib]System.Reflection.MethodBody [mscorlib]System.Reflection.MethodBase::GetMethodBody()
        callvirt instance uint8[] [mscorlib]System.Reflection.MethodBody::GetILAsByteArray()
        ldc.i4.1
        call int32 [mscorlib]System.BitConverter::ToInt32(uint8[], int32)
        stloc.s token
        ldloc.3
        call void [mscorlib]System.Console::WriteLine(object)

        ldloc.s module
        ldloc.s token
        ldloc.s typeArgs
        ldloc.s methodArgs
        callvirt instance class [mscorlib]System.Type [mscorlib]System.Reflection.Module::ResolveType(int32, class [mscorlib]System.Type[], class [mscorlib]System.Type[])
        ldloc.s expected
        call bool [mscorlib]System.Object::Equals(object, object)
        brfalse BAD_TYPE
        ldloc.s module
        ldloc.s token
        ldloc.s typeArgs
        ldloc.s methodArgs
        callvirt instance class [mscorlib]System.Reflection.MemberInfo [mscorlib]System.Reflection.Module::ResolveMember(int32, class [mscorlib]System.Type[], class [mscorlib]System.Type[])
        ldloc.s expected
        call bool [mscorlib]System.Object::Equals(object, object)
        brfalse BAD_MEMBER

        ldloc.2
        ldc.i4.1
        add
        stloc.2
    NEXT:
        ldloc.2
        ldloc.1
        ldlen
        conv.i4
        blt LOOP
        ldc.i4.0
        ret
    BAD_TYPE:
        ldc.i4.1
        ret
    BAD_MEMBER:
        ldc.i4.2
        ret
    BAD_COUNT:
        ldc.i4.3
        ret
    }
}
