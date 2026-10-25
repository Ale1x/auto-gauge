import SwiftUI

struct GaugeDialChrome: View {
    let brand: String
    let count: Int
    var body: some View {
        Canvas { context, size in
            context.scaleBy(x: size.width / 466, y: size.height / 466)
            context.stroke(Path(ellipseIn: CGRect(x: 12, y: 12, width: 442, height: 442)), with: .color(Color(red: 24/255, green: 33/255, blue: 45/255)), lineWidth: 2)
            for (index, color) in [Color(red: 82/255, green: 214/255, blue: 1), Color(red: 81/255, green: 117/255, blue: 239/255), Color(red: 239/255, green: 83/255, blue: 99/255)].enumerated() {
                context.fill(Path(CGRect(x: Double(198 + index * 25), y: 41, width: 20, height: 4)), with: .color(color))
            }
            context.draw(Text(brand).font(.custom("Montserrat-Medium", fixedSize: 32)).foregroundStyle(.white), at: CGPoint(x: 233, y: 76))
            context.fill(Path(ellipseIn: CGRect(x: 228, y: 106, width: 10, height: 10)), with: .color(Color(red: 120/255, green: 215/255, blue: 188/255)))
            let divider = Color(red: 37/255, green: 48/255, blue: 62/255)
            let lines: [CGRect] = count <= 3
                ? (count > 1 ? [CGRect(x: 92, y: 292, width: 282, height: 1)] : []) + (count == 3 ? [CGRect(x: 232, y: 308, width: 1, height: 69)] : [])
                : [CGRect(x: 232, y: 140, width: 1, height: 65), CGRect(x: 92, y: 224, width: 282, height: 1), CGRect(x: 92, y: 332, width: 282, height: 1)]
            for line in lines { context.fill(Path(line), with: .color(divider)) }
        }.accessibilityHidden(true)
    }
}
