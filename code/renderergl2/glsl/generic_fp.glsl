uniform sampler2D u_DiffuseMap;

uniform int       u_AlphaTest;
uniform int       u_IsDrawingHUD;
uniform int       u_Is2DDraw;

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

	// When rendering 3D models to HUD, ensure minimum alpha so they're visible
	// Text/2D UI should keep original alpha for proper blending
	if (u_IsDrawingHUD != 0 && u_Is2DDraw == 0)
	{
		gl_FragColor.a = max(alpha, 0.5);
	}
	else
	{
		gl_FragColor.a = alpha;
	}
}
