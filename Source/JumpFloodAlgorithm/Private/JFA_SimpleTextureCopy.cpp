// Fill out your copyright notice in the Description page of Project Settings.

#include "JFA_SimpleTextureCopy.h"

// MainPS is the entry point for the pixel shader - You can have multiple in a file but you have to specify separately
IMPLEMENT_SHADER_TYPE(, FJFA_SimpleTextureCopyPS, TEXT("/JFA_Shaders/private/JFA_SimpleTextureCopy.usf"), TEXT("MainPS"), SF_Pixel);