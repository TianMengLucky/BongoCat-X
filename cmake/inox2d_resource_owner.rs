//! Resource ownership patch for the pinned upstream OpenGL renderer.
//! The tracker also cleans up partial construction when upstream returns an error.
use std::{cell::RefCell, ops::Deref};
use glow::HasContext;

pub struct TrackedGl {
    gl: glow::Context,
    textures: RefCell<Vec<glow::Texture>>,
    buffers: RefCell<Vec<glow::Buffer>>,
    vaos: RefCell<Vec<glow::VertexArray>>,
    framebuffers: RefCell<Vec<glow::Framebuffer>>,
    programs: RefCell<Vec<glow::Program>>,
    shaders: RefCell<Vec<glow::Shader>>,
}
impl TrackedGl {
    pub fn new(gl: glow::Context) -> Self {
        Self { gl, textures: RefCell::default(), buffers: RefCell::default(),
            vaos: RefCell::default(), framebuffers: RefCell::default(),
            programs: RefCell::default(), shaders: RefCell::default() }
    }
}
impl Deref for TrackedGl {
    type Target = glow::Context;
    fn deref(&self) -> &Self::Target { &self.gl }
}
macro_rules! resource {
    ($create:ident,$delete:ident,$list:ident,$ty:ty $(,$arg:ident:$argty:ty)*)=> {
        impl TrackedGl {
            pub unsafe fn $create(&self,$($arg:$argty),*) -> Result<$ty,String> {
                let value=self.gl.$create($($arg),*)?;
                self.$list.borrow_mut().push(value);
                Ok(value)
            }
            pub unsafe fn $delete(&self,value:$ty) {
                self.$list.borrow_mut().retain(|v|*v!=value);
                self.gl.$delete(value);
            }
        }
    };
}
resource!(create_texture,delete_texture,textures,glow::Texture);
resource!(create_buffer,delete_buffer,buffers,glow::Buffer);
resource!(create_vertex_array,delete_vertex_array,vaos,glow::VertexArray);
resource!(create_framebuffer,delete_framebuffer,framebuffers,glow::Framebuffer);
resource!(create_program,delete_program,programs,glow::Program);
resource!(create_shader,delete_shader,shaders,glow::Shader,kind:u32);
impl Drop for TrackedGl {
    fn drop(&mut self) { unsafe {
        for v in self.framebuffers.get_mut().drain(..) { self.gl.delete_framebuffer(v); }
        for v in self.vaos.get_mut().drain(..) { self.gl.delete_vertex_array(v); }
        for v in self.buffers.get_mut().drain(..) { self.gl.delete_buffer(v); }
        for v in self.textures.get_mut().drain(..) { self.gl.delete_texture(v); }
        for v in self.programs.get_mut().drain(..) { self.gl.delete_program(v); }
        for v in self.shaders.get_mut().drain(..) { self.gl.delete_shader(v); }
    }}
}
