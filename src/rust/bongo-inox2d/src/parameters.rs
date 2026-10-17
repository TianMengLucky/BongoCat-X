use crate::abi::Range;
use glam::Vec2;
use serde_json::Value;
use std::{collections::HashMap, path::Path};

pub struct Parameter {
    pub minimum: Vec2,
    pub maximum: Vec2,
    pub value: Vec2,
}
pub struct Alias {
    pub name: String,
    pub axis: usize,
    pub minimum: f32,
    pub maximum: f32,
}
pub struct Parameters {
    pub values: HashMap<String, Parameter>,
    aliases: HashMap<String, Alias>,
}

impl Parameters {
    pub fn new(json: &Value, directory: &Path) -> Result<Self, String> {
        let mut values = HashMap::new();
        for param in json["param"].as_array().ok_or("Missing INP parameters")? {
            let pair = |key: &str| {
                Vec2::new(
                    param[key][0].as_f64().unwrap() as f32,
                    param[key][1].as_f64().unwrap() as f32,
                )
            };
            values.insert(
                param["name"].as_str().unwrap().to_owned(),
                Parameter {
                    minimum: pair("min"),
                    maximum: pair("max"),
                    value: pair("defaults"),
                },
            );
        }
        let path = directory.join("bongocat.bindings.json");
        let mut aliases = HashMap::new();
        if path.exists() {
            use std::io::Read;
            let mut data = Vec::new();
            std::fs::File::open(&path)
                .map_err(|e| e.to_string())?
                .take(65537)
                .read_to_end(&mut data)
                .map_err(|e| e.to_string())?;
            if data.len() > 65536 {
                return Err("Inox2D bindings exceed 64 KiB".into());
            }
            let json: Value = serde_json::from_slice(&data).map_err(|e| e.to_string())?;
            for (id, value) in json.as_object().ok_or("Bindings must be an object")? {
                let name = value["parameter"]
                    .as_str()
                    .ok_or("Missing alias parameter")?
                    .to_owned();
                let axis = match value["axis"].as_str().unwrap_or("x") {
                    "x" => 0,
                    "y" => 1,
                    _ => return Err("Alias axis must be x or y".into()),
                };
                let target = values
                    .get(&name)
                    .ok_or("Alias refers to an unknown parameter")?;
                let minimum = value["min"]
                    .as_f64()
                    .map(|v| v as f32)
                    .unwrap_or(target.minimum[axis]);
                let maximum = value["max"]
                    .as_f64()
                    .map(|v| v as f32)
                    .unwrap_or(target.maximum[axis]);
                if !minimum.is_finite() || !maximum.is_finite() || maximum <= minimum {
                    return Err("Alias range must be finite and increasing".into());
                }
                aliases.insert(
                    id.clone(),
                    Alias {
                        name,
                        axis,
                        minimum,
                        maximum,
                    },
                );
            }
        }
        Ok(Self { values, aliases })
    }
    fn resolve<'a>(
        values: &HashMap<String, Parameter>,
        aliases: &'a HashMap<String, Alias>,
        id: &'a str,
    ) -> Option<(&'a str, usize, f32, f32)> {
        if let Some(alias) = aliases.get(id) {
            return Some((&alias.name, alias.axis, alias.minimum, alias.maximum));
        }
        let (name, axis) = if values.contains_key(id) {
            (id, 0)
        } else if let Some(name) = id.strip_suffix(".x") {
            (name, 0)
        } else if let Some(name) = id.strip_suffix(".y") {
            (name, 1)
        } else {
            return None;
        };
        let parameter = values.get(name)?;
        Some((name, axis, parameter.minimum[axis], parameter.maximum[axis]))
    }
    pub fn set(&mut self, id: &str, value: f32) -> bool {
        if !value.is_finite() {
            return false;
        }
        let Some((name, axis, minimum, maximum)) = Self::resolve(&self.values, &self.aliases, id)
        else {
            return false;
        };
        let param = self.values.get_mut(name).unwrap();
        let fraction = if maximum > minimum {
            ((value - minimum) / (maximum - minimum)).clamp(0.0, 1.0)
        } else {
            0.0
        };
        param.value[axis] =
            param.minimum[axis] + fraction * (param.maximum[axis] - param.minimum[axis]);
        true
    }
    pub fn range(&self, id: &str) -> Option<Range> {
        let (name, axis, minimum, maximum) = Self::resolve(&self.values, &self.aliases, id)?;
        let param = self.values.get(name)?;
        let span = param.maximum[axis] - param.minimum[axis];
        let fraction = if span > 0.0 {
            (param.value[axis] - param.minimum[axis]) / span
        } else {
            0.0
        };
        Some(Range {
            minimum,
            maximum,
            value: minimum + fraction * (maximum - minimum),
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn borrowed_resolution_preserves_aliases_axes_and_name_precedence() {
        let mut parameters = Parameters {
            values: HashMap::from([(
                "Motion".into(),
                Parameter {
                    minimum: Vec2::new(-2.0, 10.0),
                    maximum: Vec2::new(2.0, 30.0),
                    value: Vec2::new(0.0, 20.0),
                },
            )]),
            aliases: HashMap::from([(
                "Drag".into(),
                Alias {
                    name: "Motion".into(),
                    axis: 1,
                    minimum: -1.0,
                    maximum: 1.0,
                },
            )]),
        };
        let id = String::from("Motion.x");
        let (name, axis, _, _) =
            Parameters::resolve(&parameters.values, &parameters.aliases, &id).unwrap();
        assert_eq!(name.as_ptr(), id.as_ptr());
        assert_eq!(axis, 0);
        let (name, _, _, _) =
            Parameters::resolve(&parameters.values, &parameters.aliases, "Drag").unwrap();
        assert_eq!(name.as_ptr(), parameters.aliases["Drag"].name.as_ptr());
        assert!(parameters.set("Drag", 0.25));
        assert_eq!(parameters.values["Motion"].value, Vec2::new(0.0, 22.5));
        assert_eq!(parameters.range("Drag").unwrap().value, 0.25);
        assert!(parameters.set("Drag", 2.0));
        assert_eq!(parameters.range("Motion.y").unwrap().value, 30.0);
        assert!(parameters.set("Motion.x", -1.0));
        assert_eq!(parameters.range("Motion").unwrap().value, -1.0);
        assert!(!parameters.set("Motion.z", 0.0));
        assert!(!parameters.set("Drag", f32::NAN));
        assert!(!parameters.set("Drag", f32::INFINITY));
        assert!(parameters.range("missing").is_none());
        parameters.values.insert(
            "Motion.y".into(),
            Parameter {
                minimum: Vec2::ZERO,
                maximum: Vec2::ONE,
                value: Vec2::ZERO,
            },
        );
        assert!(parameters.set("Motion.y", 0.75));
        assert_eq!(parameters.range("Motion.y").unwrap().value, 0.75);
        assert_eq!(parameters.values["Motion"].value.y, 30.0);
    }
}
