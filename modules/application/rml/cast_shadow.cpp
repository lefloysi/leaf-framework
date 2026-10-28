#include "application/rml/cast_shadow.hpp"

#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Geometry.h>
#include <RmlUi/Core/MeshUtilities.h>
#include <RmlUi/Core/PropertyDefinition.h>
#include <RmlUi/Core/RenderManager.h>
#include <algorithm>

namespace lf {

	class CastShadow final : public Rml::Decorator {
	public:
		CastShadow(Rml::Colourb color, float x, float y, float softness);
		Rml::DecoratorDataHandle GenerateElementData(Rml::Element* element, Rml::BoxArea paint_area) const override;
		void ReleaseElementData(Rml::DecoratorDataHandle handle) const override;
		void RenderElement(Rml::Element* element, Rml::DecoratorDataHandle handle) const override;

	private:
		Rml::Colourb color;
		float x;
		float y;
		float softness;
	};

	CastShadow::CastShadow(Rml::Colourb color, float x, float y, float softness) : color(color), x(x), y(y), softness(softness) {
	}

	Rml::DecoratorDataHandle CastShadow::GenerateElementData(Rml::Element* element, Rml::BoxArea paint_area) const {
		(void)paint_area;
		auto* manager = element->GetRenderManager();
		if (!manager || (x <= 0 && y <= 0)) {
			return INVALID_DECORATORDATAHANDLE;
		}

		const auto size = element->GetBox().GetSize(Rml::BoxArea::Border);
		const auto shade = color.ToPremultiplied(element->GetComputedValues().opacity());
		Rml::Mesh mesh;
		if (y > 0) {
			Rml::MeshUtilities::GenerateQuad(mesh, { x, size.y }, { size.x, y }, shade);
		}
		if (x > 0) {
			Rml::MeshUtilities::GenerateQuad(mesh, { size.x, y }, { x, std::max(0.0f, size.y - y) }, shade);
		}

		// Fade only the exposed outer edge. No offscreen layers, texture copies, or body overdraw.
		if (softness > 0 && y > 0) {
			const auto start = mesh.vertices.size();
			Rml::MeshUtilities::GenerateQuad(mesh, { x, size.y + y }, { size.x + softness, softness }, shade);
			mesh.vertices[start + 2].colour = { 0, 0, 0, 0 };
			mesh.vertices[start + 3].colour = { 0, 0, 0, 0 };
		}
		if (softness > 0 && x > 0) {
			const auto start = mesh.vertices.size();
			Rml::MeshUtilities::GenerateQuad(mesh, { size.x + x, y }, { softness, size.y }, shade);
			mesh.vertices[start + 1].colour = { 0, 0, 0, 0 };
			mesh.vertices[start + 2].colour = { 0, 0, 0, 0 };
		}

		return reinterpret_cast<Rml::DecoratorDataHandle>(new Rml::Geometry(manager->MakeGeometry(std::move(mesh))));
	}

	void CastShadow::ReleaseElementData(Rml::DecoratorDataHandle handle) const {
		delete reinterpret_cast<Rml::Geometry*>(handle);
	}

	void CastShadow::RenderElement(Rml::Element* element, Rml::DecoratorDataHandle handle) const {
		if (!handle) {
			return;
		}
		reinterpret_cast<Rml::Geometry*>(handle)->Render(element->GetAbsoluteOffset(Rml::BoxArea::Border));
	}

	CastShadowInstancer::CastShadowInstancer() {
		color_id = RegisterProperty("color", "#17111dcc").AddParser("color").GetId();
		x_id = RegisterProperty("x", "2").AddParser("number").GetId();
		y_id = RegisterProperty("y", "3").AddParser("number").GetId();
		softness_id = RegisterProperty("softness", "1").AddParser("number").GetId();
		RegisterShorthand("decorator", "color, x, y, softness", Rml::ShorthandType::FallThrough);
	}

	Rml::SharedPtr<Rml::Decorator> CastShadowInstancer::InstanceDecorator(const Rml::String& name, const Rml::PropertyDictionary& properties, const Rml::DecoratorInstancerInterface& instancer_interface) {
		(void)name;
		(void)instancer_interface;
		const auto color = properties.GetProperty(color_id)->Get<Rml::Colourb>();
		const auto x = std::clamp(properties.GetProperty(x_id)->Get<float>(), 0.0f, 16.0f);
		const auto y = std::clamp(properties.GetProperty(y_id)->Get<float>(), 0.0f, 16.0f);
		const auto softness = std::clamp(properties.GetProperty(softness_id)->Get<float>(), 0.0f, 8.0f);
		return Rml::MakeShared<CastShadow>(color, x, y, softness);
	}

}


