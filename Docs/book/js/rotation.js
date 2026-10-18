import * as THREE from 'three';
import { SVGRenderer, SVGObject } from 'three/addons/renderers/SVGRenderer.js';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

const diagramEl = document.getElementById('rotation-diagram');

const scene = new THREE.Scene();
scene.background = new THREE.Color(0xffffff);

const camera = new THREE.OrthographicCamera();

camera.position.set(9, 26, 20);
camera.up.set(0, 0, 1);
camera.lookAt(0, 0, 0);

const renderer = new SVGRenderer();
diagramEl.prepend(renderer.domElement);

const controls = new OrbitControls(camera, renderer.domElement);
controls.enableZoom = false;

const LINE_WIDTH = 4;

function createLabel(text, x, y, z, min = false) {
	const label = document.createElementNS('http://www.w3.org/2000/svg', 'text');
	label.setAttribute('class', 'label' + (min ? ' min' : ''));
	label.textContent = text;
	label.setAttribute('y', -15);
	const object = new SVGObject(label);
	object.position.x = x;
	object.position.y = y;
	object.position.z = z;
	return object;
}

function createLine(x1, y1, z1, x2, y2, z2, color) {
	const geometry = new THREE.BufferGeometry().setFromPoints([
		new THREE.Vector3(x1, y1, z1),
		new THREE.Vector3(x2, y2, z2)
	]);
	const material = new THREE.LineBasicMaterial({ color: color, linewidth: LINE_WIDTH, transparent: true, opacity: 0.8 });
	const line = new THREE.Line(geometry, material);
	scene.add(line);
	return line;
}

function changeLine(line, x1, y1, z1, x2, y2, z2) {
	line.geometry.setFromPoints([new THREE.Vector3(x1, y1, z1), new THREE.Vector3(x2, y2, z2)]);
	return line;
}

function createVector(x1, y1, z1, x2, y2, z2, color, label = '') {
	const HEAD_LENGTH = 1;
	const HEAD_WIDTH = 0.2;

	const group = new THREE.Group();
	const direction = new THREE.Vector3(x2 - x1, y2 - y1, z2 - z1).normalize();
	const norm = new THREE.Vector3(x2 - x1, y2 - y1, z2 - z1).length();
	let end = new THREE.Vector3(x2, y2, z2);

	if (norm > HEAD_LENGTH) {
		end = new THREE.Vector3(x2 - direction.x * HEAD_LENGTH / 2, y2 - direction.y * HEAD_LENGTH / 2, z2 - direction.z * HEAD_LENGTH / 2);
	}

	// create line
	const geometry = new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(x1, y1, z1), end]);
	const material = new THREE.LineBasicMaterial({ color: color, linewidth: LINE_WIDTH, transparent: true, opacity: 0.8 });
	const line = new THREE.Line(geometry, material);
	group.add(line);

	if (norm > HEAD_LENGTH) {
		// Create arrow
		const arrowGeometry = new THREE.ConeGeometry(HEAD_WIDTH, HEAD_LENGTH, 16);
		const arrowMaterial = new THREE.MeshBasicMaterial({ color: color });
		const arrow = new THREE.Mesh(arrowGeometry, arrowMaterial);
		arrow.position.set(x2 - direction.x * HEAD_LENGTH / 2, y2 - direction.y * HEAD_LENGTH / 2, z2 - direction.z * HEAD_LENGTH / 2);
		arrow.lookAt(new THREE.Vector3(x1, y1, z1));
		arrow.rotateX(-Math.PI / 2);
		group.add(arrow);
	}

	// create label
	if (label) group.add(createLabel(label, x2, y2, z2));
	scene.add(group);
	return group;
}

function changeVector(vector, x1, y1, z1, x2, y2, z2, color, label = '') {
	vector.removeFromParent();
	return createVector(x1, y1, z1, x2, y2, z2, color, label);
}

function createDrone(x, y, z) {
	const group = new THREE.Group();

	// Fuselage and wing triangle (main body)
	const fuselageGeometry = new THREE.BufferGeometry();
	const fuselageVertices = new Float32Array([
		1, 0, 0,
		-1, 0.6, 0,
		-1, -0.6, 0
	]);
	fuselageGeometry.setAttribute('position', new THREE.BufferAttribute(fuselageVertices, 3));
	const fuselageMaterial = new THREE.MeshBasicMaterial({ color: 0xb3b3b3, side: THREE.DoubleSide, transparent: true, opacity: 0.8 });
	const fuselage = new THREE.Mesh(fuselageGeometry, fuselageMaterial);
	group.add(fuselage);

	// Tail triangle
	const tailGeometry = new THREE.BufferGeometry();
	const tailVertices = new Float32Array([
		-0.2, 0, 0,
		-1, 0, 0,
		-1, 0, 0.5,
	]);
	tailGeometry.setAttribute('position', new THREE.BufferAttribute(tailVertices, 3));
	const tailMaterial = new THREE.MeshBasicMaterial({ color: 0xd80100, side: THREE.DoubleSide, transparent: true, opacity: 0.9 });
	const tail = new THREE.Mesh(tailGeometry, tailMaterial);
	group.add(tail);

	group.position.set(x, y, z);
	group.scale.set(2, 2, 2);
	scene.add(group);
	return group;
}

// Create axes
const AXES_LENGTH = 10;
createVector(0, 0, 0, AXES_LENGTH, 0, 0, 0xd80100, 'x');
createVector(0, 0, 0, 0, AXES_LENGTH, 0, 0x0076ba, 'y');
createVector(0, 0, 0, 0, 0, AXES_LENGTH, 0x57ed00, 'z');

// Rotation values
const rotationAxisSrc = new THREE.Vector3(2, 1, 3);
let rotationAngle = 0;
let rotationAxis = rotationAxisSrc.clone().normalize();
let rotationVector = new THREE.Vector3(rotationAxis.x * rotationAngle, rotationAxis.y * rotationAngle, rotationAxis.z * rotationAngle);

let rotationVectorObj = createVector(0, 0, 0, rotationVector.x, rotationVector.y, rotationVector.z, 0xff9900);
let axisObj = createLine(0, 0, 0, rotationAxis.x * AXES_LENGTH, rotationAxis.y * AXES_LENGTH, rotationAxis.z * AXES_LENGTH, 0xe8e8e8);

const drone = createDrone(0, 0, 0);

// UI
const angleInput = diagramEl.querySelector('input[name=angle]');
const rotationVectorEl = diagramEl.querySelector('.rotation-vector');
const angleEl = diagramEl.querySelector('.angle');
const quaternionEl = diagramEl.querySelector('.quaternion');
const eulerEl = diagramEl.querySelector('.euler');
diagramEl.querySelector('.axis').innerHTML = `<b style='color:#b6b6b6'>Ось вращения:</b> (${rotationAxisSrc.x}, ${rotationAxisSrc.y}, ${rotationAxisSrc.z}) ∥ (${rotationAxis.x.toFixed(1)}, ${rotationAxis.y.toFixed(1)}, ${rotationAxis.z.toFixed(1)})`;

function updateScene() {
	rotationAngle = parseFloat(angleInput.value) * Math.PI / 180;
	rotationVector.set(rotationAxis.x * rotationAngle, rotationAxis.y * rotationAngle, rotationAxis.z * rotationAngle);
	rotationVectorObj = changeVector(rotationVectorObj, 0, 0, 0, rotationVector.x, rotationVector.y, rotationVector.z, 0xff9900);

	// rotate drone
	drone.rotation.set(0, 0, 0);
	drone.rotateOnAxis(rotationAxis, rotationAngle);

	// update labels
	angleEl.innerHTML = `<b>Угол вращения:</b> ${parseFloat(angleInput.value).toFixed(0)}° = ${(rotationAngle).toFixed(2)} рад`;
	rotationVectorEl.innerHTML = `<b style='color:#e49a44'>Вектор вращения:</b> (${rotationVector.x.toFixed(1)}, ${rotationVector.y.toFixed(1)}, ${rotationVector.z.toFixed(1)}) рад`;

	let quaternion = new THREE.Quaternion();
	quaternion.setFromAxisAngle(rotationAxis, rotationAngle);

	quaternionEl.innerHTML = `<b>Кватернион:</b>
		<math xmlns="http://www.w3.org/1998/Math/MathML">
			<mrow>
				<mo>(</mo>
				<mrow>
					<mi>cos</mi>
					<mo>(</mo>
					<mfrac>
						<mi>${rotationAngle.toFixed(2)}</mi>
						<mn>2</mn>
					</mfrac>
					<mo>)</mo>
				</mrow>
				<mo>, </mo>
				<mrow>
					<mi>${rotationAxis.x.toFixed(1)}</mi>
					<mo>·</mo>
					<mi>sin</mi>
					<mo>(</mo>
					<mfrac>
						<mi>${rotationAngle.toFixed(2)}</mi>
						<mn>2</mn>
					</mfrac>
					<mo>)</mo>
				</mrow>
				<mo>, </mo>
				<mrow>
					<mi>${rotationAxis.y.toFixed(1)}</mi>
					<mo>·</mo>
					<mi>sin</mi>
					<mo>(</mo>
					<mfrac>
						<mi>${rotationAngle.toFixed(2)}</mi>
						<mn>2</mn>
					</mfrac>
					<mo>)</mo>
				</mrow>
				<mo>,</mo>
				<mrow>
					<mi>${rotationAxis.z.toFixed(1)}</mi>
					<mo>·</mo>
					<mi>sin</mi>
					<mo>(</mo>
					<mfrac>
						<mi>${rotationAngle.toFixed(2)}</mi>
						<mn>2</mn>
					</mfrac>
					<mo>)</mo>
				</mrow>
				<mo>)</mo>
			</mrow>
		</math>
		= (${quaternion.w.toFixed(1)}, ${(quaternion.x).toFixed(1)}, ${(quaternion.y).toFixed(1)}, ${(quaternion.z).toFixed(1)})`;

	eulerEl.innerHTML = `<b>Углы Эйлера:</b> крен ${(drone.rotation.x * 180 / Math.PI).toFixed(0)}°,
		 тангаж ${(drone.rotation.y * 180 / Math.PI).toFixed(0)}°, рыскание ${(drone.rotation.z * 180 / Math.PI).toFixed(0)}°`;
}

function updateCamera() {
	const RANGE = 8;
	const VERT_SHIFT = 2;
	const HOR_SHIFT = -2;
	const width = renderer.domElement.clientWidth;
	const height = renderer.domElement.clientHeight;
	const ratio = width / height;
	if (ratio > 1) {
		camera.left = -RANGE * ratio;
		camera.right = RANGE * ratio;
		camera.top = RANGE + VERT_SHIFT;
		camera.bottom = -RANGE + VERT_SHIFT;
	} else {
		camera.left = -RANGE + HOR_SHIFT;
		camera.right = RANGE + HOR_SHIFT;
		camera.top = RANGE / ratio + VERT_SHIFT;
		camera.bottom = -RANGE / ratio + VERT_SHIFT;
	}
	camera.updateProjectionMatrix();
	renderer.setSize(width, height);
}

function update() {
	// requestAnimationFrame(update);
	updateCamera();
	updateScene();
	controls.update();
	renderer.render(scene, camera);
}
update();

window.addEventListener('resize', update);
angleInput.addEventListener('input', update);
angleInput.addEventListener('change', update);
diagramEl.addEventListener('mousemove', update);
diagramEl.addEventListener('touchmove', update);
diagramEl.addEventListener('scroll', update);
diagramEl.addEventListener('wheel', update);
