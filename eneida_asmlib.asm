format MS64 COFF

extrn '__imp_RtlFillMemory' as RtlFillMemory:qword
public memset
public asm_sinf
public asm_cosf

section '.text' code readable executable

memset:
    sub rsp, 8
    mov r9d, edx
    mov rdx, r8
    mov r8d, r9d
    call [RtlFillMemory]
    add rsp, 8
    ret

asm_sinf:
    vmovaps xmm1, xmm0
    vmulss xmm0, xmm0, [k_f32_reciprocal_two_pi]
    vroundss xmm0, xmm0, xmm0, 0
    vmulss xmm0, xmm0, [k_f32_two_pi]
    vsubss xmm0, xmm1, xmm0
    vandps xmm1, xmm0, [k_i32_negative_zero]
    vorps xmm2, xmm1, [k_f32_pi]
    vandnps xmm3, xmm1, xmm0
    vsubss xmm4, xmm2, xmm0
    vcmpless xmm5, xmm3, [k_f32_half_pi]
    vblendvps xmm0, xmm4, xmm0, xmm5
    vmulss xmm1, xmm0, xmm0
    vmovss xmm2, [k_f32_sin_coefficients+16]
    vfmadd213ss xmm2, xmm1, [k_f32_sin_coefficients+12]
    vfmadd213ss xmm2, xmm1, [k_f32_sin_coefficients+8]
    vfmadd213ss xmm2, xmm1, [k_f32_sin_coefficients+4]
    vfmadd213ss xmm2, xmm1, [k_f32_sin_coefficients]
    vfmadd213ss xmm2, xmm1, [k_f32_one]
    vmulss xmm0, xmm2, xmm0
    ret

asm_cosf:
  .k_stack_size = 8+32
    sub rsp, .k_stack_size
    vmovaps [rsp], xmm6
    vmovaps [rsp+16], xmm7
    vmovaps xmm1, xmm0
    vmulss xmm0, xmm0, [k_f32_reciprocal_two_pi]
    vroundss xmm0, xmm0, xmm0,0
    vmulss xmm0, xmm0, [k_f32_two_pi]
    vsubss xmm0, xmm1, xmm0
    vandps xmm1, xmm0, [k_i32_negative_zero]
    vorps xmm2, xmm1, [k_f32_pi]
    vandnps xmm3, xmm1, xmm0
    vsubss xmm4, xmm2, xmm0
    vcmpless xmm5, xmm3, [k_f32_half_pi]
    vblendvps xmm0, xmm4, xmm0, xmm5
    vmovaps xmm6, [k_f32_one]
    vandnps xmm2, xmm5, [k_f32_negative_one]
    vandps xmm1, xmm5, xmm6
    vorps xmm7, xmm1, xmm2
    vmulss xmm1, xmm0, xmm0
    vmovss xmm2, [k_f32_cos_coefficients+16]
    vfmadd213ss xmm2, xmm1, [k_f32_cos_coefficients+12]
    vfmadd213ss xmm2, xmm1, [k_f32_cos_coefficients+8]
    vfmadd213ss xmm2, xmm1, [k_f32_cos_coefficients+4]
    vfmadd213ss xmm2, xmm1, [k_f32_cos_coefficients]
    vfmadd213ss xmm2, xmm1, xmm6
    vmulss xmm0, xmm2, xmm7
    vmovaps xmm6, [rsp]
    vmovaps xmm7, [rsp+16]
    add rsp, .k_stack_size
    ret

section '.data' data readable writeable

k_f32_reciprocal_two_pi: dd 8 dup 0.159154943
k_f32_two_pi: dd 8 dup 6.283185307
k_i32_negative_zero: dd 8 dup 0x80000000
k_f32_negative_one: dd 8 dup -1.0
k_f32_pi: dd 8 dup 3.141592654
k_f32_half_pi: dd 8 dup 1.570796327
k_f32_one: dd 8 dup 1.0
k_f32_sin_coefficients: dd -0.16666667,0.0083333310,-0.00019840874,2.7525562e-06,-2.3889859e-08,-0.16665852,0.0083139502,-0.00018524670
k_f32_cos_coefficients: dd -0.5,0.041666638,-0.0013888378,2.4760495e-05,-2.6051615e-07,-0.49992746,0.041493919,-0.0012712436
