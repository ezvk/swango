#include <drm_fourcc.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static uint32_t output_formats_8bit[] = {
	DRM_FORMAT_XRGB8888, DRM_FORMAT_XBGR8888, DRM_FORMAT_RGBX8888,
	DRM_FORMAT_BGRX8888, DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888,
	DRM_FORMAT_RGBA8888, DRM_FORMAT_BGRA8888, DRM_FORMAT_RGB888,
	DRM_FORMAT_BGR888,
};

static uint32_t output_formats_10bit[] = {
	DRM_FORMAT_XRGB2101010, DRM_FORMAT_XBGR2101010, DRM_FORMAT_RGBX1010102,
	DRM_FORMAT_BGRX1010102, DRM_FORMAT_ARGB2101010, DRM_FORMAT_ABGR2101010,
	DRM_FORMAT_RGBA1010102, DRM_FORMAT_BGRA1010102,
};

static bool output_set_render_format(Monitor *m, uint32_t candidates[],
									 size_t count,
									 struct wlr_output_state *state) {
	for (size_t i = 0; i < count; i++) {
		struct wlr_output_state test_state = *state;
		wlr_output_state_set_render_format(&test_state, candidates[i]);
		if (wlr_output_test_state(m->wlr_output, &test_state)) {
			wlr_output_state_set_render_format(state, candidates[i]);
			return true;
		}
	}

	wlr_log(WLR_DEBUG, "HDR: Failed to set render format");
	return false;
}

static bool output_format_in_candidates(uint32_t format, uint32_t candidates[],
										size_t count) {
	for (size_t i = 0; i < count; i++)
		if (candidates[i] == format)
			return true;
	return false;
}

static enum render_bit_depth bit_depth_from_format(uint32_t render_format) {
	if (output_format_in_candidates(render_format, output_formats_10bit,
									ARRAY_SIZE(output_formats_10bit)))
		return MANGO_RENDER_BIT_DEPTH_10;
	if (output_format_in_candidates(render_format, output_formats_8bit,
									ARRAY_SIZE(output_formats_8bit)))
		return MANGO_RENDER_BIT_DEPTH_8;
	return MANGO_RENDER_BIT_DEPTH_DEFAULT;
}

static bool output_supports_hdr(const Monitor *m, const char **reason) {
	const struct wlr_output *output = m->wlr_output;
	const char *r = NULL;

	// The first two checks are derived from the EDID. Some panels declare their
	// HDR capability only inside a DisplayID 2.0 extension with the CTA-861
	// blocks nested in a container (tag 0x81) -- legal EDID 1.4, but wlroots
	// reads it through libdisplay-info's CTA path and comes back empty, so
	// supported_primaries/supported_transfer_functions are 0 on a panel that is
	// perfectly capable of PQ. `hdr_force:1` in the monitorrule is the escape
	// hatch for exactly that case (Hyprland calls it `supports_hdr = 1`).
	//
	// The third check is NOT forceable: output_color_transform is a real
	// renderer capability, not an EDID claim. Under the GLES renderer it is
	// false and no config key can make BT.2020/PQ output work -- HDR needs the
	// Vulkan renderer (WLR_RENDERER=vulkan).
	if (!m->hdr_force &&
		!(output->supported_primaries & WLR_COLOR_NAMED_PRIMARIES_BT2020))
		r = "BT2020 primaries not supported";
	else if (!m->hdr_force && !(output->supported_transfer_functions &
								WLR_COLOR_TRANSFER_FUNCTION_ST2084_PQ))
		r = "PQ transfer function not supported";
	else if (!drw->features.output_color_transform)
		r = "renderer doesn't support output color transforms";
	if (reason)
		*reason = r;
	return !r;
}

void output_enable_hdr(Monitor *m, struct wlr_output_state *os, bool enabled,
					   bool silent) {

	if (!m->is_hdr_enabling && !enabled)
		return;

	if (!output_supports_hdr(m, NULL)) {
		m->is_hdr_enabling = false;
		return;
	}

	if (!enabled) {
		if (m->wlr_output->supported_primaries ||
			m->wlr_output->supported_transfer_functions) {
			if (!silent)
				wlr_log(WLR_DEBUG, "Disabling HDR on output %s",
						m->wlr_output->name);
			wlr_output_state_set_image_description(os, NULL);
		}
		m->is_hdr_enabling = false;
		return;
	}

	if (!silent)
		wlr_log(WLR_DEBUG, "Enabling HDR on output %s", m->wlr_output->name);
	struct wlr_output_image_description desc = {
		.primaries = WLR_COLOR_NAMED_PRIMARIES_BT2020,
		.transfer_function = WLR_COLOR_TRANSFER_FUNCTION_ST2084_PQ,
	};

	// Mastering display metadata. Optional per wlr_output.h, but leaving it at
	// zero -- which a bare designated initializer does -- means the panel gets
	// an HDR_OUTPUT_METADATA infoframe whose primaries are 0.0 and whose
	// luminances are 0. Measured on a Samsung ATNA40CU05-0 (ROG Zephyrus G14):
	// mango, sway and labwc all sent 0/0/0 while Hyprland sent 616/616/400 with
	// BT.2020 primaries and a D65 white point. The display has to tone-map
	// against *something*, and zeros tell it nothing about the content.
	//
	// wlroots does not hand us the EDID luminances (wlr_output only exposes
	// supported_primaries / supported_transfer_functions), so the values come
	// from the monitorrule rather than from a second EDID parse. That also
	// sidesteps the DisplayID problem entirely: a panel whose HDR block is
	// unreachable can still be described correctly by hand.
	//
	// Luminances are in cd/m² (wlr_output.h). 0 means "unset" and is left as
	// such, so a config that does not mention them behaves exactly as before.
	if (m->hdr_max_lum > 0) {
		wlr_color_primaries_from_named(&desc.mastering_display_primaries,
									   WLR_COLOR_NAMED_PRIMARIES_BT2020);
		desc.mastering_luminance.min = m->hdr_min_lum;
		desc.mastering_luminance.max = m->hdr_max_lum;
		// max_cll defaults to the mastering peak; max_fall is the frame-average
		// ceiling and is meaningless without an explicit value, so it stays 0
		// unless configured.
		desc.max_cll = m->hdr_max_lum;
		desc.max_fall = m->hdr_max_avg_lum;
		if (!silent)
			wlr_log(WLR_DEBUG,
					"HDR: mastering luminance %.4f-%.0f cd/m², max_cll %.0f, "
					"max_fall %.0f on %s",
					desc.mastering_luminance.min, desc.mastering_luminance.max,
					desc.max_cll, desc.max_fall, m->wlr_output->name);
	}

	m->is_hdr_enabling = true;
	wlr_output_state_set_image_description(os, &desc);
}

void output_state_setup_hdr(Monitor *m, bool silent,
							struct wlr_output_state *state) {
	uint32_t render_format = m->wlr_output->render_format;
	const char *unsupported_reason = NULL;
	bool hdr_supported = output_supports_hdr(m, &unsupported_reason);
	bool hdr_succeeded = false;

	if (!hdr_supported) {
		if (!silent)
			wlr_log(WLR_INFO, "HDR not supported on output %s: %s",
					m->wlr_output->name, unsupported_reason);
		return;
	}

	enum render_bit_depth depth = config.hdr_depth;
	if (depth == MANGO_RENDER_BIT_DEPTH_DEFAULT)
		depth = bit_depth_from_format(render_format);

	if (depth == MANGO_RENDER_BIT_DEPTH_10 &&
		bit_depth_from_format(render_format) == depth) {
		hdr_succeeded = true; // 上次已经成功设置10位，直接复用
	} else if (depth == MANGO_RENDER_BIT_DEPTH_10) {
		hdr_succeeded = output_set_render_format(
			m, output_formats_10bit, ARRAY_SIZE(output_formats_10bit), state);
		if (!hdr_succeeded) {
			if (!silent)
				wlr_log(WLR_INFO,
						"No 10 bit color formats supported, HDR disabled.");
			hdr_succeeded = output_set_render_format(
				m, output_formats_8bit, ARRAY_SIZE(output_formats_8bit), state);
			if (!hdr_succeeded) {
				if (!silent)
					wlr_log(WLR_ERROR, "No 8 bit color formats supported!");
			}
		}
	} else {
		// 明确要求8位或自动降级
		hdr_succeeded = output_set_render_format(
			m, output_formats_8bit, ARRAY_SIZE(output_formats_8bit), state);
		if (!hdr_succeeded) {
			if (!silent)
				wlr_log(WLR_ERROR, "No 8 bit color formats supported!");
		}
	}

	output_enable_hdr(m, state, hdr_succeeded, silent);
}