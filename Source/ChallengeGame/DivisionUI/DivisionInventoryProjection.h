#pragma once

#include "CoreMinimal.h"

// One projective transform shared by the UI material and pointer routing.
// Corners are TL, TR, BR, BL, expressed in the presentation widget's UV space.
struct FDivisionInventoryProjection
{
    double Forward[3][3] = {{1,0,0}, {0,1,0}, {0,0,1}};
    double Inverse[3][3] = {{1,0,0}, {0,1,0}, {0,0,1}};

    bool SetQuad(const FVector2D& TL, const FVector2D& TR,
        const FVector2D& BR, const FVector2D& BL)
    {
        const FVector2D A = TR - BR;
        const FVector2D B = BL - BR;
        const FVector2D C = TL - TR + BR - BL;
        const double Denominator = A.X * B.Y - B.X * A.Y;
        if (FMath::Abs(Denominator) < 1.e-10) return false;
        const double G = (C.X * B.Y - B.X * C.Y) / Denominator;
        const double H = (A.X * C.Y - C.X * A.Y) / Denominator;
        Forward[0][0] = TR.X - TL.X + G * TR.X;
        Forward[0][1] = BL.X - TL.X + H * BL.X;
        Forward[0][2] = TL.X;
        Forward[1][0] = TR.Y - TL.Y + G * TR.Y;
        Forward[1][1] = BL.Y - TL.Y + H * BL.Y;
        Forward[1][2] = TL.Y;
        Forward[2][0] = G;
        Forward[2][1] = H;
        Forward[2][2] = 1;
        const auto& M = Forward;
        const double Det = M[0][0] * (M[1][1]*M[2][2] - M[1][2]*M[2][1])
            - M[0][1] * (M[1][0]*M[2][2] - M[1][2]*M[2][0])
            + M[0][2] * (M[1][0]*M[2][1] - M[1][1]*M[2][0]);
        if (FMath::Abs(Det) < 1.e-10) return false;
        for (int32 Row = 0; Row < 3; ++Row)
            for (int32 Col = 0; Col < 3; ++Col)
                Inverse[Col][Row] = (M[(Row+1)%3][(Col+1)%3]*M[(Row+2)%3][(Col+2)%3]
                    - M[(Row+1)%3][(Col+2)%3]*M[(Row+2)%3][(Col+1)%3]) / Det;
        return true;
    }

    bool ScreenToLogical(FVector2D UV, FVector2D& Logical) const
    {
        const double W = Inverse[2][0]*UV.X + Inverse[2][1]*UV.Y + Inverse[2][2];
        if (FMath::Abs(W) < 1.e-8) return false;
        Logical.X = (Inverse[0][0]*UV.X + Inverse[0][1]*UV.Y + Inverse[0][2]) / W;
        Logical.Y = (Inverse[1][0]*UV.X + Inverse[1][1]*UV.Y + Inverse[1][2]) / W;
        const bool bInside = Logical.X >= 0 && Logical.X <= 1 && Logical.Y >= 0 && Logical.Y <= 1;
        Logical *= FVector2D(1200, 900);
        return bInside;
    }

    FLinearColor MaterialRow(int32 Row) const
    {
        return FLinearColor(Inverse[Row][0], Inverse[Row][1], Inverse[Row][2], 0);
    }
};
