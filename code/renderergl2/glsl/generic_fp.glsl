uniform sampler2D u_DiffuseMap;

uniform int       u_AlphaTest;
uniform int       u_IsDrawingHUD;
uniform int       u_Is2DDraw;
uniform int       u_IsBlending;

varying vec2      var_DiffuseTex;

varying vec4      var_Color;


void main()
{
	vec4 color  = texture2D(u_DiffuseMap, var_DiffuseTex);

	float alpha = color.a * var_Color.a;
	if (u_AlphaTest == 1)
	{
		if (alpha == 0.0)
			discard;
	}
	else if (u_AlphaTest == 2)
	{
		if (alpha >= 0.5)
			discard;
	}
	else if (u_AlphaTest == 3)
	{
		if (alpha < 0.5)
			discard;
	}

	gl_FragColor.rgb = color.rgb * var_Color.rgb;

	// For HUD 3D models: if blending is enabled, use natural alpha for proper
	// multi-stage specular effects (glBlendFuncSeparate handles final opacity).
	// If not blending (single-stage), force opaque to avoid transparent models.
	if (u_IsDrawingHUD != 0 && u_Is2DDraw == 0 && u_IsBlending == 0)
	{
		gl_FragColor.a = 1.0;
	}
	else
	{
		gl_FragColor.a = alpha;
	}
}
