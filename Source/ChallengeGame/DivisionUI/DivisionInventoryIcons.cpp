#include "DivisionInventoryIcons.h"

#include "Framework/Application/SlateApplication.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/SlateRenderer.h"
#include "Rendering/SlateResourceHandle.h"
#include "Styling/CoreStyle.h"

#include <initializer_list>

namespace
{
    // Original, code-authored contours. No image assets or font glyphs are involved.
    struct FIconMesh
    {
        FVector2f Canvas = FVector2f(100.0f, 100.0f);
        TArray<FVector2f> Points;
        TArray<SlateIndex> Indices;
        // Keep source contours beside the triangulated fill. Custom Slate
        // vertices have no AA switch, while MakeLines can provide an AA fringe.
        TArray<TArray<FVector2f>> Contours;

        static float Cross(FVector2f A, FVector2f B, FVector2f C)
        {
            return (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
        }

        // Ear clipping supports concave garment/weapon outlines. Never use a fan
        // for a concave contour: that would fill the negative-space details.
        void Polygon(std::initializer_list<FVector2f> Outline)
        {
            TArray<FVector2f> P;
            for (const FVector2f V : Outline) P.Add(V);
            const int32 Base = Points.Num();
            const int32 FirstIndex = Indices.Num();
            TArray<int32> Remaining;
            float Area = 0.0f;
            for (int32 I = 0; I < P.Num(); ++I)
            {
                Remaining.Add(I);
                const FVector2f A = P[I], B = P[(I + 1) % P.Num()];
                Area += A.X * B.Y - A.Y * B.X;
            }
            const float Winding = Area >= 0.0f ? 1.0f : -1.0f;
            Points.Append(P);
            const int32 FirstContour = Contours.Num();
            if (P.Num() >= 2)
            {
                TArray<FVector2f> Edge = P;
                Edge.Add(P[0]);
                Contours.Add(MoveTemp(Edge));
            }
            while (Remaining.Num() > 2)
            {
                bool bClipped = false;
                for (int32 I = 0; I < Remaining.Num(); ++I)
                {
                    const int32 A = Remaining[(I + Remaining.Num() - 1) % Remaining.Num()];
                    const int32 B = Remaining[I];
                    const int32 C = Remaining[(I + 1) % Remaining.Num()];
                    if (Winding * Cross(P[A], P[B], P[C]) <= UE_SMALL_NUMBER) continue;
                    bool bContainsPoint = false;
                    for (const int32 J : Remaining)
                    {
                        if (J == A || J == B || J == C) continue;
                        if (Winding * Cross(P[A], P[B], P[J]) >= -UE_SMALL_NUMBER &&
                            Winding * Cross(P[B], P[C], P[J]) >= -UE_SMALL_NUMBER &&
                            Winding * Cross(P[C], P[A], P[J]) >= -UE_SMALL_NUMBER)
                        {
                            bContainsPoint = true;
                            break;
                        }
                    }
                    if (bContainsPoint) continue;
                    Indices.Add(static_cast<SlateIndex>(Base + A));
                    Indices.Add(static_cast<SlateIndex>(Base + B));
                    Indices.Add(static_cast<SlateIndex>(Base + C));
                    Remaining.RemoveAt(I);
                    bClipped = true;
                    break;
                }
                if (!ensureMsgf(bClipped, TEXT("Invalid inventory icon contour")))
                {
                    Points.SetNum(Base);
                    Indices.SetNum(FirstIndex);
                    Contours.SetNum(FirstContour);
                    return;
                }
            }
        }

        // Annular strips have actual transparent centers, including partial arcs.
        void Arc(FVector2f Center, float Radius, float Width, float Start, float Sweep)
        {
            const int32 Steps = FMath::Max(2, FMath::CeilToInt(FMath::Abs(Sweep) / 7.5f));
            const int32 Base = Points.Num();
            TArray<FVector2f> Outer;
            TArray<FVector2f> Inner;
            Outer.Reserve(Steps + 1);
            Inner.Reserve(Steps + 1);
            for (int32 I = 0; I <= Steps; ++I)
            {
                const float Angle = FMath::DegreesToRadians(Start + Sweep * I / Steps);
                const FVector2f Direction(FMath::Cos(Angle), FMath::Sin(Angle));
                const FVector2f OuterPoint = Center + Direction * Radius;
                const FVector2f InnerPoint = Center + Direction * (Radius - Width);
                Points.Add(OuterPoint);
                Points.Add(InnerPoint);
                Outer.Add(OuterPoint);
                Inner.Add(InnerPoint);
            }
            for (int32 I = 0; I < Steps; ++I)
            {
                const int32 A = Base + I * 2;
                for (const int32 Index : { A, A + 2, A + 1, A + 1, A + 2, A + 3 })
                    Indices.Add(static_cast<SlateIndex>(Index));
            }
            Contours.Add(MoveTemp(Outer));
            Contours.Add(MoveTemp(Inner));
        }

        void Disc(FVector2f Center, float Radius)
        {
            const int32 Base = Points.Add(Center);
            constexpr int32 Steps = 32;
            TArray<FVector2f> Edge;
            Edge.Reserve(Steps + 1);
            for (int32 I = 0; I < Steps; ++I)
            {
                const float Angle = 2.0f * PI * I / Steps;
                const FVector2f Point = Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
                Points.Add(Point);
                Edge.Add(Point);
            }
            for (int32 I = 0; I < Steps; ++I)
            {
                Indices.Add(static_cast<SlateIndex>(Base));
                Indices.Add(static_cast<SlateIndex>(Base + 1 + I));
                Indices.Add(static_cast<SlateIndex>(Base + 1 + (I + 1) % Steps));
            }
            // Copy before Add: Add may reallocate Edge, so passing Edge[0]
            // directly would alias the container being modified.
            const FVector2f FirstPoint = Edge[0];
            Edge.Add(FirstPoint);
            Contours.Add(MoveTemp(Edge));
        }
    };

    const TMap<FName, FIconMesh>& Atlas()
    {
        static const TMap<FName, FIconMesh> Meshes = []
        {
            TMap<FName, FIconMesh> Result;
            auto Weapon = [&Result](const TCHAR* Name) -> FIconMesh&
            {
                FIconMesh& M = Result.Add(FName(Name));
                M.Canvas = FVector2f(100.0f, 60.0f);
                return M;
            };

            {
                FIconMesh& M = Weapon(TEXT("Rifle"));
                // Adjustable buttstock, upper/lower receiver, ventilated fore-end.
                M.Polygon({{3,24},{17,24},{23,28},{29,28},{29,34},{19,34},{8,43},{3,43}});
                M.Polygon({{29,24},{52,24},{55,27},{73,27},{73,33},{53,33},{50,37},{29,37}});
                M.Polygon({{54,22},{75,22},{75,25},{54,25}});
                for (int32 X = 55; X < 73; X += 6)
                    M.Polygon({{float(X),25},{float(X+3),25},{float(X+3),27},{float(X),27}});
                M.Polygon({{75,27},{93,27},{93,25},{98,25},{98,32},{93,32},{93,30},{75,30}});
                M.Polygon({{31,37},{38,37},{36,49},{29,47}});
                M.Polygon({{45,37},{54,35},{55,44},{59,51},{50,54},{46,46}});
                M.Polygon({{38,39},{40,39},{40,43},{44,43},{44,37},{46,37},{46,45},{38,45}});
                M.Polygon({{31,24},{31,19},{35,19},{35,24}});
                M.Polygon({{70,22},{71,17},{74,17},{75,22}});
            }
            {
                FIconMesh& M = Weapon(TEXT("SMG"));
                // Retracting wire stock, short shroud, straight magazine and stub muzzle.
                M.Polygon({{6,23},{28,23},{28,26},{10,26},{10,37},{6,39}});
                M.Polygon({{10,33},{28,28},{28,31},{10,36}});
                M.Polygon({{28,21},{62,21},{68,25},{79,25},{79,33},{62,35},{28,35}});
                M.Polygon({{31,35},{40,35},{37,48},{29,46}});
                M.Polygon({{52,35},{61,35},{62,55},{53,55}});
                M.Polygon({{40,37},{42,37},{42,42},{48,42},{48,35},{50,35},{50,44},{39,44}});
                M.Polygon({{81,26},{90,26},{90,24},{94,24},{94,32},{81,32}});
                M.Polygon({{33,21},{33,17},{37,17},{37,21}});
                M.Polygon({{72,25},{73,19},{77,19},{77,25}});
                M.Polygon({{63,37},{69,36},{68,45},{63,45}});
            }
            {
                FIconMesh& M = Weapon(TEXT("Shotgun"));
                // Swept shoulder stock, long tube magazine, ribbed sliding pump.
                M.Polygon({{3,29},{20,29},{29,25},{45,25},{45,33},{31,33},{25,37},{20,37},{8,46},{3,46}});
                M.Polygon({{45,24},{98,24},{98,28},{45,28}});
                M.Polygon({{46,30},{91,30},{91,33},{46,33}});
                M.Polygon({{54,35},{77,35},{77,39},{54,39}});
                for (int32 X = 54; X < 78; X += 4)
                    M.Polygon({{float(X),33},{float(X+2),33},{float(X+2),35},{float(X),35}});
                M.Polygon({{30,35},{32,35},{32,40},{40,40},{40,33},{43,33},{43,43},{30,43}});
                M.Polygon({{92,24},{93,21},{95,21},{95,24}});
            }
            {
                FIconMesh& M = Weapon(TEXT("LMG"));
                // Heavy fixed stock, box ammunition, carry handle and deployed bipod.
                M.Polygon({{2,24},{18,24},{25,28},{28,28},{28,36},{20,36},{7,43},{2,43}});
                M.Polygon({{28,24},{60,24},{64,28},{76,28},{76,36},{28,36}});
                M.Polygon({{35,24},{35,16},{54,16},{58,20},{58,24},{54,24},{54,21},{51,19},{39,19},{39,24}});
                M.Polygon({{76,27},{94,27},{94,25},{99,25},{99,33},{94,33},{94,31},{76,31}});
                M.Polygon({{42,38},{60,38},{64,43},{62,53},{42,53}});
                M.Polygon({{30,36},{37,36},{35,49},{28,47}});
                M.Polygon({{78,33},{81,33},{89,51},{86,52}});
                M.Polygon({{77,35},{79,39},{72,52},{69,51}});
                M.Polygon({{68,28},{69,21},{73,21},{74,28}});
                M.Polygon({{61,38},{64,38},{66,41},{64,43}});
            }
            {
                FIconMesh& M = Weapon(TEXT("Pistol"));
                // Squared slide, exposed hammer, raked grip and open trigger guard.
                M.Polygon({{25,15},{78,15},{82,19},{82,27},{29,27},{24,23}});
                M.Polygon({{29,29},{76,29},{76,33},{48,33},{43,37},{37,54},{21,51},{29,33}});
                M.Polygon({{48,35},{51,35},{49,41},{59,41},{62,33},{66,33},{62,45},{45,45}});
                M.Polygon({{25,15},{22,11},{26,9},{31,15}});
                M.Polygon({{33,15},{33,11},{37,11},{37,15}});
                M.Polygon({{73,15},{74,11},{78,11},{78,15}});
            }
            {
                FIconMesh& M = Result.Add(TEXT("Vest"));
                // Shoulder straps frame the neck; separate plates leave fabric seams.
                M.Polygon({{22,10},{37,10},{40,22},{47,27},{47,45},{23,45},{23,32},{16,28}});
                M.Polygon({{63,10},{78,10},{84,28},{77,32},{77,45},{53,45},{53,27},{60,22}});
                // Separate webbing bands and ammunition pouches leave real seams.
                M.Polygon({{23,48},{77,48},{79,54},{21,54}});
                M.Polygon({{21,57},{79,57},{81,62},{19,62}});
                M.Polygon({{19,65},{35,65},{35,84},{21,84},{18,78}});
                M.Polygon({{38,65},{62,65},{62,84},{38,84}});
                M.Polygon({{65,65},{81,65},{82,78},{79,84},{65,84}});
                M.Polygon({{21,87},{79,87},{77,92},{23,92}});
                M.Polygon({{10,48},{20,48},{17,60},{10,60}});
                M.Polygon({{80,48},{90,48},{90,60},{83,60}});
            }
            {
                FIconMesh& M = Result.Add(TEXT("Mask"));
                // Gas-mask crown, open twin lenses, bridge, respirator and filters.
                M.Polygon({{23,33},{25,19},{37,10},{63,10},{75,19},{77,33},{70,28},{62,25},{38,25},{30,28}});
                M.Arc({34,40}, 15, 5, 0, 360);
                M.Arc({66,40}, 15, 5, 0, 360);
                M.Polygon({{48,30},{52,30},{55,49},{62,57},{57,68},{43,68},{38,57},{45,49}});
                M.Polygon({{22,55},{31,57},{39,69},{61,69},{69,57},{78,55},{73,77},{61,88},{39,88},{27,77}});
                M.Arc({18,64}, 11, 6, 0, 360);
                M.Arc({82,64}, 11, 6, 0, 360);
            }
            {
                FIconMesh& M = Result.Add(TEXT("Backpack"));
                M.Polygon({{37,18},{37,8},{63,8},{63,18},{58,18},{58,13},{42,13},{42,18}});
                M.Polygon({{25,25},{35,20},{65,20},{75,25},{78,49},{22,49}});
                M.Polygon({{22,53},{28,53},{28,80},{72,80},{72,53},{78,53},{78,78},{71,91},{29,91},{22,78}});
                M.Polygon({{32,54},{68,54},{68,61},{32,61}});
                M.Polygon({{32,64},{68,64},{68,76},{32,76}});
                M.Polygon({{8,45},{18,41},{18,80},{11,80},{8,74}});
                M.Polygon({{82,41},{92,45},{92,74},{89,80},{82,80}});
                M.Polygon({{27,94},{36,94},{36,99},{27,99}});
                M.Polygon({{64,94},{73,94},{73,99},{64,99}});
            }
            {
                FIconMesh& M = Result.Add(TEXT("Kneepads"));
                // Paired articulated cups, with separate upper/lower fastening straps.
                for (const float X : { 0.0f, 46.0f })
                {
                    M.Polygon({{X+14,23},{X+34,23},{X+41,34},{X+38,59},{X+31,67},{X+17,67},{X+10,59},{X+7,34}});
                    M.Polygon({{X+12,65},{X+18,71},{X+30,71},{X+36,65},{X+34,80},{X+28,87},{X+20,87},{X+14,80}});
                    M.Polygon({{X+3,18},{X+45,18},{X+45,22},{X+3,22}});
                    M.Polygon({{X+3,47},{X+7,47},{X+8,56},{X+3,56}});
                    M.Polygon({{X+41,47},{X+45,47},{X+45,56},{X+40,56}});
                }
            }
            {
                FIconMesh& M = Result.Add(TEXT("Gloves"));
                // Two five-finger gloves. Finger separations are contour notches.
                M.Polygon({{14,69},{8,52},{5,41},{7,37},{11,38},{17,49},{17,22},{20,18},{24,20},{24,40},{27,40},{27,13},{30,10},{34,13},{34,40},{37,40},{37,17},{40,15},{44,18},{44,44},{47,44},{47,26},{50,24},{53,27},{53,56},{46,72}});
                M.Polygon({{16,74},{44,77},{43,90},{16,87}});
                M.Polygon({{61,69},{57,58},{57,31},{60,28},{63,30},{63,47},{66,47},{66,22},{69,19},{72,22},{72,46},{75,46},{75,19},{78,16},{82,19},{82,47},{85,47},{85,28},{88,25},{91,28},{91,55},{94,47},{98,47},{99,52},{93,69},{87,78}});
                M.Polygon({{62,74},{86,82},{83,93},{59,85}});
            }
            {
                FIconMesh& M = Result.Add(TEXT("Holster"));
                // Drop-leg rig with belt loops, two thigh straps and shaped pistol pouch.
                M.Polygon({{22,8},{78,8},{78,17},{22,17}});
                M.Polygon({{34,20},{43,20},{43,30},{34,30}});
                M.Polygon({{58,20},{67,20},{67,30},{58,30}});
                M.Polygon({{32,33},{69,33},{76,44},{69,80},{59,93},{40,89},{30,69}});
                M.Polygon({{9,39},{28,39},{28,48},{9,48}});
                M.Polygon({{77,39},{91,39},{91,48},{78,48}});
                M.Polygon({{12,67},{27,67},{30,76},{12,76}});
                M.Polygon({{74,67},{90,67},{90,76},{72,76}});
                M.Polygon({{46,20},{55,20},{57,30},{46,30}});
            }
            {
                FIconMesh& M = Result.Add(TEXT("Talent"));
                // Broken hexagonal badge enclosing a solid lightning bolt.
                M.Polygon({{9,29},{46,7},{46,14},{15,33},{15,67},{46,86},{46,93},{9,71}});
                M.Polygon({{54,7},{91,29},{91,71},{54,93},{54,86},{85,67},{85,33},{54,14}});
                M.Polygon({{48,21},{67,21},{54,44},{70,44},{35,81},{44,55},{30,55}});
            }
            {
                FIconMesh& M = Result.Add(TEXT("Target"));
                for (int32 I = 0; I < 4; ++I) M.Arc({50,50}, 33, 5, I*90.0f+12, 66);
                M.Arc({50,50}, 19, 4, 0, 360);
                M.Disc({50,50}, 7);
                M.Polygon({{47,3},{53,3},{53,28},{47,28}});
                M.Polygon({{47,72},{53,72},{53,97},{47,97}});
                M.Polygon({{3,47},{28,47},{28,53},{3,53}});
                M.Polygon({{72,47},{97,47},{97,53},{72,53}});
            }
            {
                FIconMesh& M = Result.Add(TEXT("Shield"));
                // Four separated solid plates retain a narrow cross-shaped opening.
                M.Polygon({{12,16},{34,13},{47,7},{47,43},{17,43}});
                M.Polygon({{53,7},{66,13},{88,16},{83,43},{53,43}});
                M.Polygon({{18,49},{47,49},{47,94},{31,83},{23,68}});
                M.Polygon({{53,49},{82,49},{77,68},{69,83},{53,94}});
            }
            {
                FIconMesh& M = Result.Add(TEXT("Biohazard"));
                // Three open lobes and disconnected inner ring: all holes are empty.
                M.Arc({50,29}, 24, 7, -55, 290);
                M.Arc({31.813f,60.5f}, 24, 7, 65, 290);
                M.Arc({68.187f,60.5f}, 24, 7, 185, 290);
                for (int32 I = 0; I < 3; ++I) M.Arc({50,50}, 12, 4, I*120.0f+15, 90);
            }
            {
                FIconMesh& M = Result.Add(TEXT("Currency"));
                M.Arc({50,50}, 43, 6, 0, 360);
                // Angular credit C and interrupted central stroke.
                M.Polygon({{67,29},{39,29},{29,39},{29,61},{39,71},{67,71},{67,63},{43,63},{38,58},{38,42},{43,37},{67,37}});
                M.Polygon({{47,19},{53,19},{53,26},{47,26}});
                M.Polygon({{47,74},{53,74},{53,81},{47,81}});
                M.Polygon({{45,46},{70,46},{70,53},{45,53}});
            }
            return Result;
        }();
        return Meshes;
    }
}

void DivisionInventoryIcons::Draw(FName Icon, const FGeometry& Geometry,
    FSlateWindowElementList& Elements, int32 Layer, FVector2D Position,
    FVector2D Size, FLinearColor Tint)
{
    if (Size.X <= 0.0 || Size.Y <= 0.0 || Tint.A <= 0.0f) return;
    const FIconMesh* Mesh = Atlas().Find(Icon);
    if (!Mesh) return;

    const float Scale = static_cast<float>(FMath::Min(Size.X / Mesh->Canvas.X, Size.Y / Mesh->Canvas.Y));
    const FVector2f Origin = FVector2f(Position) + (FVector2f(Size) - Mesh->Canvas * Scale) * 0.5f;
    const FColor Color = Tint.ToFColorSRGB();
    TArray<FSlateVertex> Vertices;
    Vertices.Reserve(Mesh->Points.Num());
    for (const FVector2f Point : Mesh->Points)
    {
        // Custom vertices are already in window space: include the full geometry
        // render transform (DPI, parent scale/rotation), exactly once.
        Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
            Geometry.GetAccumulatedRenderTransform(), Origin + Point * Scale,
            FVector2f(0.5f, 0.5f), Color));
    }

    // Draw an antialiased contour behind the fill. This covers polygon, annulus
    // and disc boundaries; the high-resolution target then filters the remaining
    // custom-vertex coverage when the perspective material samples it.
    for (const TArray<FVector2f>& Contour : Mesh->Contours)
    {
        if (Contour.Num() < 2) continue;
        TArray<FVector2f> EdgePoints;
        EdgePoints.Reserve(Contour.Num());
        for (const FVector2f Point : Contour)
            EdgePoints.Add(Origin + Point * Scale);
        FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(),
            MoveTemp(EdgePoints), ESlateDrawEffect::None, Tint, true, 1.0f);
    }

    // WhiteBrush is an engine FSlateColorBrush with NoImage. A null resource proxy
    // is intentional for this brush; Slate renders it with its white fallback.
    const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
    const FSlateResourceHandle Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Brush);
    FSlateDrawElement::MakeCustomVerts(Elements, Layer + 1, Resource, Vertices,
        Mesh->Indices, nullptr, 0, 0, ESlateDrawEffect::None);
}
